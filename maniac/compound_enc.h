/*//////////////////////////////////////////////////////////////////////////////////////////////////////

Copyright 2010-2016, Jon Sneyers & Pieter Wuille
Copyright 2019, Jon Sneyers, Cloudinary (jon@cloudinary.com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

//////////////////////////////////////////////////////////////////////////////////////////////////////*/

#pragma once
#include <math.h>

// leaf nodes during tree construction phase
template <typename BitChance, int bits> class CompoundSymbolChances final : public FinalCompoundSymbolChances<BitChance, bits> {
public:
    std::vector<std::pair<SymbolChance<BitChance, bits>,SymbolChance<BitChance, bits> > > virtChances;
    uint64_t realSize;
    std::vector<uint64_t> virtSize;
    std::vector<int64_t> virtPropSum;
    int32_t count;
    int16_t best_property;
//    Ranges range;


    void resetCounters() {
        best_property = -1;
        realSize = 0;
        count = 0;
        virtPropSum.assign(virtPropSum.size(),0);
        virtSize.assign(virtSize.size(),0);
    }

    CompoundSymbolChances(int nProp, //Ranges ranges, 
        uint16_t zero_chance) :
        FinalCompoundSymbolChances<BitChance, bits>(zero_chance),
        virtChances(nProp,std::make_pair(SymbolChance<BitChance, bits>(zero_chance), SymbolChance<BitChance,bits>(zero_chance))),
        realSize(0),
        virtSize(nProp),
        virtPropSum(nProp),
        count(0),
        best_property(-1)
//        range(ranges)
    { }

};

template <typename BitChance, typename RAC, int bits>
void inline FinalCompoundSymbolBitCoder<BitChance,RAC,bits>::write(const bool bit, const SymbolChanceBitType type, const int i) {
        BitChance& ch = chances.realChances.bit(type, i);
        rac.write_12bit_chance(ch.get_12bit(), bit);
        updateChances(type, i, bit);
    }


// This function is currently not used, but it could be used to estimate the current cost of writing a number without actually writing it.
// (that could be useful for a lossy encoder)
template <typename BitChance, typename RAC, int bits>
void inline FinalCompoundSymbolBitCoder<BitChance,RAC,bits>::estimate(const bool bit, const SymbolChanceBitType type, const int i, uint64_t &total) {
        BitChance& ch = chances.realChances.bit(type, i);
        ch.estim(bit, total);
    }



template <typename BitChance, typename RAC, int bits> class CompoundSymbolBitCoder {
public:
    typedef typename BitChance::Table Table;

private:
    const Table &table;
    RAC &rac;
    CompoundSymbolChances<BitChance, bits> &chances;
    std::vector<bool> &select;

    void inline updateChances(SymbolChanceBitType type, int i, bool bit) {
        BitChance& real = chances.realChances.bit(type,i);
        real.estim(bit, chances.realSize);
        real.put(bit, table);

        int16_t best_property = -1;
        uint64_t best_size = chances.realSize;
        for (unsigned int j=0; j<chances.virtChances.size(); j++) {
            BitChance& virt = (select)[j] ? chances.virtChances[j].first.bit(type,i)
                              : chances.virtChances[j].second.bit(type,i);
            virt.estim(bit, chances.virtSize[j]);
            virt.put(bit, table);
            if (chances.virtSize[j] < best_size) {
                best_size = chances.virtSize[j];
                best_property = j;
            }
        }
        chances.best_property = best_property;
    }
    BitChance inline & bestChance(SymbolChanceBitType type, int i = 0) {
        signed short int p = chances.best_property;
        return (p == -1 ? chances.realChances.bit(type,i)
                : ((select)[p] ? chances.virtChances[p].first.bit(type,i)
                   : chances.virtChances[p].second.bit(type,i) ));
    }

public:
    CompoundSymbolBitCoder(const Table &tableIn, RAC &racIn, CompoundSymbolChances<BitChance, bits> &chancesIn, std::vector<bool> &selectIn) : table(tableIn), rac(racIn), chances(chancesIn), select(selectIn) {}

    bool read(SymbolChanceBitType type, int i = 0) {
        BitChance& ch = bestChance(type, i);
        bool bit = rac.read_12bit_chance(ch.get_12bit());
        updateChances(type, i, bit);
        return bit;
    }

    void write(bool bit, SymbolChanceBitType type, int i = 0) {
        BitChance& ch = bestChance(type, i);
        rac.write_12bit_chance(ch.get_12bit(), bit);
        updateChances(type, i, bit);
    }

    void estimate(const bool bit, const SymbolChanceBitType type, const int i, uint64_t &total) {
        BitChance& ch = bestChance(type, i);
        ch.estim(bit, total);
    }

};


template <typename BitChance, typename RAC, int bits>
FinalCompoundSymbolChances<BitChance,bits> inline & FinalPropertySymbolCoder<BitChance,RAC,bits>::find_leaf_readonly(const Properties &properties) {
        Tree::size_type pos = 0;
//        while(inner_node[pos].property != -1 && inner_node[pos].count < 0) {
        while(inner_node[pos].property != -1) { // && inner_node[pos].count < 0) {
                if (properties[inner_node[pos].property] > inner_node[pos].splitval) {
                  pos = inner_node[pos].childID;
                } else {
                  pos = inner_node[pos].childID+1;
                }
        }
//        return leaf_node[inner_node[pos].leafID];
        return leaf_node[inner_node[pos].childID];
    }


template <typename BitChance, typename RAC, int bits>
void FinalCompoundSymbolCoder<BitChance,RAC,bits>::write_int(FinalCompoundSymbolChances<BitChance, bits>& chancesIn, int min, int max, int val) {
        FinalCompoundSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, rac, chancesIn);
        writer<bits>(bitCoder, min, max, val);
    }

template <typename BitChance, typename RAC, int bits>
int FinalCompoundSymbolCoder<BitChance,RAC,bits>::estimate_int(FinalCompoundSymbolChances<BitChance, bits>& chancesIn, int min, int max, int val) {
        FinalCompoundSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, rac, chancesIn);
        return estimate_writer<bits>(bitCoder, min, max, val);
    }

template <typename BitChance, typename RAC, int bits>
void FinalCompoundSymbolCoder<BitChance,RAC,bits>::write_int(FinalCompoundSymbolChances<BitChance, bits>& chancesIn, int nbits, int val) {
        FinalCompoundSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, rac, chancesIn);
        writer(bitCoder, nbits, val);
    }



template <typename BitChance, typename RAC, int bits> class CompoundSymbolCoder {
private:
    typedef typename CompoundSymbolBitCoder<BitChance, RAC, bits>::Table Table;
    RAC &rac;
    const Table table;

public:

    CompoundSymbolCoder(RAC& racIn, int cut = 2, int alpha = 0xFFFFFFFF / 19) : rac(racIn), table(cut,alpha) {}

    int read_int(CompoundSymbolChances<BitChance, bits> &chancesIn, std::vector<bool> &selectIn, int min, int max) {
        if (min == max) { return min; }
        CompoundSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, rac, chancesIn, selectIn);
        return reader<bits>(bitCoder, min, max);
    }

    void write_int(CompoundSymbolChances<BitChance, bits>& chancesIn, std::vector<bool> &selectIn, int min, int max, int val) {
        if (min == max) { assert(val==min); return; }
        CompoundSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, rac, chancesIn, selectIn);
        writer<bits>(bitCoder, min, max, val);
    }

    int estimate_int(CompoundSymbolChances<BitChance, bits>& chancesIn, std::vector<bool> &selectIn, int min, int max, int val) {
        if (min == max) { assert(val==min); return 0; }
        CompoundSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, rac, chancesIn, selectIn);
        return estimate_writer<bits>(bitCoder, min, max, val);
    }


    int read_int(CompoundSymbolChances<BitChance, bits> &chancesIn, std::vector<bool> &selectIn, int nbits) {
        CompoundSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, rac, chancesIn, selectIn);
        return reader(bitCoder, nbits);
    }

    void write_int(CompoundSymbolChances<BitChance, bits>& chancesIn, std::vector<bool> &selectIn, int nbits, int val) {
        CompoundSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, rac, chancesIn, selectIn);
        writer(bitCoder, nbits, val);
    }
};



template <typename BitChance, typename RAC, int bits>
void FinalPropertySymbolCoder<BitChance,RAC,bits>::write_int(const Properties &properties, int min, int max, int val) {
        if (min == max) { assert(val==min); return; }
        assert(properties.size() == nb_properties);
        FinalCompoundSymbolChances<BitChance,bits> &chances = find_leaf(properties);
        coder.write_int(chances, min, max, val);
    }

template <typename BitChance, typename RAC, int bits>
int FinalPropertySymbolCoder<BitChance,RAC,bits>::estimate_int(const Properties &properties, int min, int max, int val) {
        if (min == max) { assert(val==min); return 0; }
        assert(properties.size() == nb_properties);
        FinalCompoundSymbolChances<BitChance,bits> &chances = find_leaf_readonly(properties);
        return coder.estimate_int(chances, min, max, val);
    }

template <typename BitChance, typename RAC, int bits>
void FinalPropertySymbolCoder<BitChance,RAC,bits>::write_int(const Properties &properties, int nbits, int val) {
        assert(properties.size() == nb_properties);
        FinalCompoundSymbolChances<BitChance,bits> &chances = find_leaf(properties);
        coder.write_int(chances, nbits, val);
    }


template <typename BitChance, typename RAC, int bits> class PropertySymbolCoder {
public:
    typedef CompoundSymbolCoder<BitChance, RAC, bits> Coder;
private:
    RAC &rac;
    Coder coder;
    const Ranges range;
    unsigned int nb_properties;
    std::vector<CompoundSymbolChances<BitChance,bits> > leaf_node;
    Tree &inner_node;
    std::vector<bool> selection;
    int split_threshold;

    inline PropertyVal div_down(int64_t sum, int32_t count) const {
        assert(count > 0);
        if (sum >= 0) return sum/count;
        else return -((-sum + count-1)/count);
    }
    inline PropertyVal compute_splitval(const CompoundSymbolChances<BitChance,bits> &ch, int16_t p, const Ranges &crange) const {
        // encourage making initial branches for > 0, == 0, < 0
/*
        if (ch.range[p].first <= 0 && ch.range[p].second >= 0) {
            if (ch.range[p].second > 0) return 0;
            if (ch.range[p].first < 0) return -1;
        }
*/
        // make first branch on > 0
/*
        if (ch.range[p].first < 0 && ch.range[p].second > 0) {
            return 0;
        }
*/
        if (crange[p].first < 0 && crange[p].second > 0) {
            return 0;
        }
        PropertyVal splitval = div_down(ch.virtPropSum[p],ch.count);
//        if (splitval >= ch.range[p].second)
//             splitval = ch.range[p].second-1; // == does happen because of rounding and running average
        if (splitval >= crange[p].second)
             splitval = crange[p].second-1; // == does happen because of rounding and running average
        return splitval;
    }


    CompoundSymbolChances<BitChance,bits> inline &find_leaf_readonly(const Properties &properties) {
        uint32_t pos = 0;
        Ranges current_ranges = range;
        while(inner_node[pos].property != -1) {
            if (properties[inner_node[pos].property] > inner_node[pos].splitval) {
                current_ranges[inner_node[pos].property].first = inner_node[pos].splitval + 1;
                pos = inner_node[pos].childID;
            } else {
                current_ranges[inner_node[pos].property].second = inner_node[pos].splitval;
                pos = inner_node[pos].childID+1;
            }
        }
//        CompoundSymbolChances<BitChance,bits> &result = leaf_node[inner_node[pos].leafID];
        CompoundSymbolChances<BitChance,bits> &result = leaf_node[inner_node[pos].childID];
        set_selection(properties,result,current_ranges);
        return result;
    }

    CompoundSymbolChances<BitChance,bits> inline &find_leaf(const Properties &properties) {
        uint32_t pos = 0;
        Ranges current_ranges = range;
        while(inner_node[pos].property != -1) {
            if (properties[inner_node[pos].property] > inner_node[pos].splitval) {
                current_ranges[inner_node[pos].property].first = inner_node[pos].splitval + 1;
                pos = inner_node[pos].childID;
            } else {
                current_ranges[inner_node[pos].property].second = inner_node[pos].splitval;
                pos = inner_node[pos].childID+1;
            }
        }
//        CompoundSymbolChances<BitChance,bits> &result = leaf_node[inner_node[pos].leafID];
        CompoundSymbolChances<BitChance,bits> &result = leaf_node[inner_node[pos].childID];
        set_selection_and_update_property_sums(properties,result,current_ranges);

        // split leaf node if some virtual context is performing (significantly) better
        if(result.best_property != -1
           && result.realSize > result.virtSize[result.best_property] + split_threshold
           && leaf_node.size() < 0xFFFF && inner_node.size() < 0xFFFF
           && current_ranges[result.best_property].first < current_ranges[result.best_property].second) {

          int16_t p = result.best_property;
          PropertyVal splitval = compute_splitval(result,p,current_ranges);
//            printf("splitting on property %i, splitval %i (pos=%i)\n",p,splitval,pos);
/*
          PropertyVal splitval = div_down(result.virtPropSum[p],result.count);
          if (splitval >= current_ranges[result.best_property].second)
             splitval = current_ranges[result.best_property].second-1; // == does happen because of rounding and running average
*/

          uint32_t new_inner = inner_node.size();
          inner_node.push_back(inner_node[pos]);
          inner_node.push_back(inner_node[pos]);
          inner_node[pos].splitval = splitval;
//            fprintf(stdout,"Splitting on property %i, splitval=%i (count=%i)\n",p,inner_node[pos].splitval, (int)result.count);
          inner_node[pos].property = p;
//          if (result.count < INT16_MAX) inner_node[pos].count = result.count;
//          else inner_node[pos].count = INT16_MAX;
          uint32_t new_leaf = leaf_node.size();
          result.resetCounters();
          leaf_node.push_back(CompoundSymbolChances<BitChance,bits>(result));
//          uint32_t old_leaf = inner_node[pos].leafID;
          uint32_t old_leaf = inner_node[pos].childID;
          inner_node[pos].childID = new_inner;
//          inner_node[new_inner].leafID = old_leaf;
//          inner_node[new_inner+1].leafID = new_leaf;
          inner_node[new_inner].childID = old_leaf;
          inner_node[new_inner+1].childID = new_leaf;
//          leaf_node[old_leaf].range[p].first = splitval+1;
//          leaf_node[new_leaf].range[p].second = splitval;
          if (properties[p] > inner_node[pos].splitval) {
                return leaf_node[old_leaf];
          } else {
                return leaf_node[new_leaf];
          }
        }
        return result;
    }

    void inline set_selection_and_update_property_sums(const Properties &properties, CompoundSymbolChances<BitChance,bits> &chances, const Ranges &crange) {
        chances.count++;
        for(unsigned int i=0; i<nb_properties; i++) {
            assert(properties[i] >= range[i].first);
            assert(properties[i] <= range[i].second);
            chances.virtPropSum[i] += properties[i];
//            PropertyVal splitval = div_down(chances.virtPropSum[i],chances.count);
            PropertyVal splitval = compute_splitval(chances,i,crange);
            selection[i] = (properties[i] > splitval);
        }
    }
    void inline set_selection(const Properties &properties, const CompoundSymbolChances<BitChance,bits> &chances, const Ranges &crange) {
        if (chances.count == 0) return;
        for(unsigned int i=0; i<nb_properties; i++) {
            assert(properties[i] >= range[i].first);
            assert(properties[i] <= range[i].second);
//            PropertyVal splitval = div_down(chances.virtPropSum[i],chances.count);
            PropertyVal splitval = compute_splitval(chances,i,crange);
            selection[i] = (properties[i] > splitval);
        }
    }

public:
    PropertySymbolCoder(RAC& racIn, Ranges &rangeIn, Tree &treeIn, int zero_chance=ZERO_CHANCE, int st=CONTEXT_TREE_SPLIT_THRESHOLD, int cut = 2, int alpha = 0xFFFFFFFF / 19) :
        rac(racIn),
        coder(racIn, cut, alpha),
        range(rangeIn),
        nb_properties(range.size()),
        leaf_node(1,CompoundSymbolChances<BitChance,bits>(nb_properties,zero_chance)),
        inner_node(treeIn),
        selection(nb_properties,false),
        split_threshold(st) {

/*        leaf_node[0].realChances.bitZero().set_12bit(zero_chance);
        for(unsigned int i=0; i<nb_properties; i++) {
            leaf_node[0].virtChances[i].first.bitZero().set_12bit(zero_chance);
            leaf_node[0].virtChances[i].second.bitZero().set_12bit(zero_chance);
        }
*/
    }

    int read_int(Properties &properties, int min, int max) {
//        CompoundSymbolChances<BitChance,bits> &chances = find_leaf(properties);
//        set_selection_and_update_property_sums(properties,chances);
        CompoundSymbolChances<BitChance,bits> &chances2 = find_leaf(properties);
        return coder.read_int(chances2, selection, min, max);
    }

    void write_int(Properties &properties, int min, int max, int val) {
//        CompoundSymbolChances<BitChance,bits> &chances = find_leaf(properties);
//        set_selection_and_update_property_sums(properties,chances);
        CompoundSymbolChances<BitChance,bits> &chances2 = find_leaf(properties);
        coder.write_int(chances2, selection, min, max, val);
    }

    int estimate_int(Properties &properties, int min, int max, int val) {
        CompoundSymbolChances<BitChance,bits> &chances = find_leaf_readonly(properties);
//        set_selection(properties,chances);
        return coder.estimate_int(chances, selection, min, max, val);
    }

    int read_int(Properties &properties, int nbits) {
//        CompoundSymbolChances<BitChance,bits> &chances = find_leaf(properties);
//        set_selection_and_update_property_sums(properties,chances);
        CompoundSymbolChances<BitChance,bits> &chances2 = find_leaf(properties);
        return coder.read_int(chances2, selection, nbits);
    }

    void write_int(Properties &properties, int nbits, int val) {
//        CompoundSymbolChances<BitChance,bits> &chances = find_leaf(properties);
//        set_selection_and_update_property_sums(properties,chances);
        CompoundSymbolChances<BitChance,bits> &chances2 = find_leaf(properties);
        coder.write_int(chances2, selection, nbits, val);
    }

    void kill_children(int pos) {
        PropertyDecisionNode &n = inner_node[pos];
        if (n.property == -1) n.property = 0;
        else kill_children(n.childID);
        PropertyDecisionNode &n1 = inner_node[pos+1];
        if (n1.property == -1) n1.property = 0;
        else kill_children(n1.childID);
    }
    // destructive simplification procedure, prunes subtrees with too low counts
    long long int simplify_subtree(int pos, int divisor, int min_size, int indent) {
        PropertyDecisionNode &n = inner_node[pos];
        if (n.property == -1) {
            for (int i=0;i<indent;i++) v_printf(10,"  ");
            v_printf(10,"* leaf: count=%lli, size=%llu bits, bits per int: %f\n", (long long int)leaf_node[n.childID].count, (unsigned long long int)leaf_node[n.childID].realSize/5461, (leaf_node[n.childID].count > 0 ? leaf_node[n.childID].realSize/leaf_node[n.childID].count*1.0/5461 : -1));
/*
            if (leaf_node[n.childID].count > 100) {
            for (int i=0;i<indent;i++) v_printf(10,"  ");
            v_printf(10,"  chances: ");
            v_printf(10," zero=%i",leaf_node[n.childID].realChances.bit_zero);
            v_printf(10," sign=%i",leaf_node[n.childID].realChances.bit_sign);
            for (int i=0; i<5; i++)
            v_printf(10," exp%i=%i",i,leaf_node[n.childID].realChances.bit_exp[i]);
            v_printf(10," mant=");
            for (int i=0; i<8; i++)
            v_printf(10,"%i ",leaf_node[n.childID].realChances.bit_mant[i]);
            v_printf(10,"\n");
            }
*/
            if (leaf_node[n.childID].count == 0) return -100; // avoid empty leafs by giving them an extra penalty
            return leaf_node[n.childID].count;


        } else {
            for (int i=0;i<indent;i++) v_printf(10,"  ");
            v_printf(10,"* test: property %i, value > %i ?\n", n.property, n.splitval);
            long long int subtree_size = 0;
            subtree_size += simplify_subtree(n.childID, divisor, min_size, indent+1);
            subtree_size += simplify_subtree(n.childID+1, divisor, min_size, indent+1);
/*
            n.count /= divisor;
            n.count = pow(n.count,CONTEXT_TREE_COUNT_POWER);
            if (n.count > CONTEXT_TREE_MAX_COUNT) {
               n.count = CONTEXT_TREE_MAX_COUNT;
            }
            if (n.count < CONTEXT_TREE_MIN_COUNT_ENCODER) n.count=CONTEXT_TREE_MIN_COUNT_ENCODER;
            if (n.count < CONTEXT_TREE_MIN_COUNT) n.count=CONTEXT_TREE_MIN_COUNT;
            if (n.count > 0xf) n.count &= 0xfff8; // remove some lsb entropy
//            assert(n.count == -1);
//            printf("%li COUNT\n",n.count);
*/
            if (subtree_size < min_size) {
                for (int i=0;i<indent;i++) v_printf(11,"  ");
                v_printf(11,"[PRUNING THE ABOVE SUBTREE]\n");
                n.property = -1; // procedure is destructive because the childID is not set
                kill_children(n.childID);
            }
//            else v_printf(9,"PROPERTY %i\n",n.property);
            return subtree_size;
        }
    }
    void simplify(int divisor=CONTEXT_TREE_COUNT_DIV, int min_size=CONTEXT_TREE_MIN_SUBTREE_SIZE) {
        v_printf(10,"MANIAC TREE BEFORE SIMPLIFICATION:\n");
        simplify_subtree(0, divisor, min_size, 0);
    }
    uint64_t compute_total_size_subtree(int pos) {
        PropertyDecisionNode &n = inner_node[pos];
        uint64_t total=0;
        if (n.property == -1) {
            total += leaf_node[n.childID].realSize/5461;
        } else {
            total += compute_total_size_subtree(n.childID);
            total += compute_total_size_subtree(n.childID+1);
        }
        return total;
    }
    uint64_t compute_total_size() {
        return compute_total_size_subtree(0);
    }
};



template <typename BitChance, typename RAC>
void MetaPropertySymbolCoder<BitChance,RAC>::write_subtree(int pos, Ranges &subrange, const Tree &tree) {
        const PropertyDecisionNode &n = tree[pos];
        int p = n.property;
        coder[0].write_int2(0,nb_properties,p+1);
        if (p != -1) {
//            coder[1].write_int2(CONTEXT_TREE_MIN_COUNT, CONTEXT_TREE_MAX_COUNT, n.count);
//            printf("From properties 0..%i, split node at PROPERTY %i\n",nb_properties-1,p);
            int oldmin = subrange[p].first;
            int oldmax = subrange[p].second;
            assert(oldmin < oldmax);
            coder[2].write_int2(oldmin, oldmax-1, n.splitval);
//            e_printf( "Pos %i: prop %i splitval %i in [%i..%i]\n", pos, n.property, n.splitval, oldmin, oldmax-1);
            // > splitval
            subrange[p].first = n.splitval+1;
            write_subtree(n.childID, subrange, tree);

            // <= splitval
            subrange[p].first = oldmin;
            subrange[p].second = n.splitval;
            write_subtree(n.childID+1, subrange, tree);

            subrange[p].second = oldmax;
        }
    }
template <typename BitChance, typename RAC>
void MetaPropertySymbolCoder<BitChance,RAC>::write_tree(const Tree &tree) {
          //fprintf(stdout,"Saving tree with %lu nodes.\n",tree.size());
          Ranges rootrange(range);
          write_subtree(0, rootrange, tree);
    }
