
# FUIF: Free Universal Image Format

**WARNING: This is a research prototype. The bitstream is not finalized. Images encoded
with the current version of FUIF may not (and probably will not) decode with future versions
of FUIF. Use at your own risk!**

FUIF is a new image format, combining ideas from JPEG, lossless WebP, and FLIF.
It is a 'universal' image format in several senses:

1. it works well for any kind of image (photographic and non-photographic)
2. it can do both lossless and lossy (with the same underlying algorithm)
3. it is Responsive By Design: one file can be used, instead of needing
   separate images for the low-quality image placeholder, thumbnail, preview, dpr 1, dpr 2, etc
4. it is backwards compatible with JPEG, in the sense that existing JPEG images can be
   transcoded to FUIF losslessly (no generation loss) and effectively (smaller files)
5. it can achieve various trade-offs between encode/decode complexity and compression density


In more detail:

## 1. Any kind of image

FUIF supports several methods for image preprocessing and entropy coding, so it can use
the methods that work best for a particular image (or a particular part of an image).

For non-photographic images with few colors, a palette (color index) can be used.
There is no limit on the palette size.

For images with repetition (e.g. images that include text, where the same letter shapes
appear in multiple locations), an optional transformation can be used to replace repetition
with references (somewhat similar to JBIG2).

For photographic images, the DCT transformation can be used.

For raw sensor data, the format supports an arbitrary number of channels, that do not need
to have the same dimensions, and that do not need to have the same value ranges either.
The actual ranges can be used effectively (not just 8-bit or 16-bit like in PNG, but also
things like 14-bit with a black level and white level range of 1023..15600).

FUIF supports bit depths up to 28-bit per channel, unsigned or signed.
Only integer values are supported (no floating point), but in principle, floating point
numbers can be represented as integers plus a suitable transfer function.


## 2. Lossless and lossy

FUIF uses reversible transforms (YCoCg, reversible Haar-like squeezing);
optional quantization is the only source of loss.

Unlike FLIF, FUIF was designed with lossy compression in mind.
Unlike JPEG, FUIF can obtain fully lossless compression.
Unlike WebP and JPEG 2000, FUIF uses the same algorithm for both lossy and lossless.

FUIF attempts to offer state-of-the-art compression for both lossy and lossless compression.


## 3. Responsive By Design


The FUIF bitstream is designed in such a way that truncating a file results in a
lower resolution downscaled image. It is similar to progressive JPEG or JPEG 2000
in this respect. Image formats (like WebP, HEIC/BPG, AVIF) that are derived from a video codec
(VP8, HEVC, AV1) intra-frame encoding, do not support progressive decoding, since
in a video codec it does not make much sense to decode a single frame progressively.

FUIF was designed specifically with web delivery in mind. Instead of having to produce,
store and deliver many variants of an image, scaled to various dimensions, the idea is
to use a single file and truncate it depending on the dimensions it gets rendered on.
This approach has several advantages:

* Less storage needed (no redundancy)
* Better for CDNs (cache hit is more likely)
* Less bandwidth needed (e.g. if you are currently sending a LQIP, then a thumbnail, then when it gets clicked a larger image)
* Smart browsers could make choices themselves, e.g. load lower resolution when on a slow or expensive connection; when the local browser cache
  gets too full, instead of deleting whole files, it could trim off bytes at the end, keeping a preview in cache

The header of a FUIF file contains a mandatory list of truncation offsets, which makes
it easy to know how many bytes should be requested or served. The following offsets are
included in the header:

* LQIP: the first very low-quality preview of an image (typically at 100-200 bytes)
* scale 1/16
* scale 1/8
* scale 1/4
* scale 1/2

These power-of-two downscales are exact, in the sense that if you truncate the file at the given offset,
the resulting image is the same as decoding the whole full-resolution image and then downscaling it.
This cannot really be done in an efficient way with progressive JPEG.

FUIF has a minimalistic, compact header layout, so the first bits of actual image data appear as
early as possible. This makes it possible to get a LQIP within a small byte budget, while it is still
the beginning of the actual full image, so you also get the actual image dimensions, truncation offsets
etc.


## 4. Backwards compatible with JPEG

There are a LOT of existing JPEG images out there, as well as devices that produce new JPEG images.
If the only way to transcode a JPEG image to a new image format, is to decode the JPEG to pixels and
encode that to the new format, then there is a problem. Either you get significant generation loss,
or you get new files that are actually larger than the original.

FUIF supports the 8x8 DCT transform (which is inherently lossy due to rounding errors) and the
YCbCr color transform (which is also inherently lossy for the same reason). This means it can
losslessly represent the actual information in a JPEG image, while still adding most of the benefits
of FUIF:

* LQIP and scale 1/16 (progressive JPEG starts at scale 1/8)
* Minimal header overhead
* Better compression

Comparing FUIF to [Dropbox's Lepton](https://github.com/dropbox/lepton), which also does lossless JPEG recompression:

* Lepton can obtain slightly better compression density
* Lepton is faster
* Lepton is bit-exact (not just image-exact)
* FUIF can be decoded progressively (Lepton is anti-progressive since it encodes the AC before the DC)
* FUIF can do more than just what JPEG can, for example you can add an alpha channel to an existing JPEG


## 5. Trade-offs between complexity and compression density

The entropy coding of FUIF is based on MANIAC (just like FLIF).
Different trade-offs between computational complexity and compression density can be obtained.
Using predefined or restricted MANIAC trees (or even no trees at all), encoding can be made
faster; if encode time is not an issue, then there are many ways to optimize the encoding.

In principle, subsets of the formats ("profiles") could be defined
(e.g. by using fixed or restricted transformations and MANIAC trees)
for which both encoding and decoding can be specialized (or done in hardware),
making it significantly faster.
These subsets are not currently defined nor implemented.

FUIF is also well-suited for adaptive compression (i.e. having different qualities in
different regions of the image). The MANIAC tree can represent an arbitrary segmentation
of the image; there is no notion of (having to align with) macroblocks. This makes it easy
to effectively use different context models for different regions of the image.


## FUIF Design Principles

The following goals or principles have guided the design of FUIF:

* FUIF is an __image format, not a container__. FUIF stores pixels and the metadata needed to
render the image, but that's it. Anything else, like Exif metadata, comments,
rotation, crop coordinates, layers, tiling, image sequences, and so on is a task
for the container format (e.g. HEIF), not for the image format.
FUIF does support simple 'filmstrip' animation, so it can be used to recompress GIF and APNG,
but it is not a video codec.
_Rationale: this is a matter of separation of concerns, as well as avoiding duplication of functionality.
For simple animations, FUIF might be suitable, but in general we think that most animations
should be encoded as a short (looping) video
(taking advantage of inter-frame prediction, which is out of the scope of an image format)._

* FUIF is optimized for __delivery__, not storage. It is progressive / "responsive by design". One single file
can be used instead of having to downscale a high-resolution original to various target resolutions.
The bitstream does not require seeking and a meaningful placeholder/preview can be shown from the first few
hundred bytes. Perhaps it is possible to achieve better compression density and/or faster encoding by not
optimizing for delivery (e.g. by predicting DC from AC like Lepton does).
If the use case is (essentially) write-only archival then maybe it is worth it to sacrifice progressive decoding,
and FUIF can in principle be used in such a way, but that's not the primary target.
_Rationale: the variety of display devices (from smart watches to huge 8K screens) and network conditions
(from 2G on the road in rural areas to very high speed at home) is a huge challenge.
Making many files for the same image is not the best approach, e.g. in terms of CDN cache hits.
Moreover browsers get few opportunities to be smart and adjust the image resolution and quality
automatically based on the device and network conditions. Non-progressive image formats result
in a bad user experience when bandwidth is an issue._

* __'Honest' compression artifacts__.
There are various approaches to how to do lossy compression. Different techniques lead to different artifacts.
Some artifacts are less "annoying" than other artifacts. For example, blurring, smearing and mild ringing are probably
not very annoying (or even desireable to some, because it might eliminate noise and increase perceived sharpness),
while pixelation, blockiness and color banding are annoying and obvious compression artifacts.
Also, some artifacts are not very "honest", in the sense that the image looks deceptively better than
it actually is. For example, JBIG2 in lossy mode or HEIC at low bitrates can produce images that look like they
are high-quality (e.g. they have sharp details at the pixel level),
but they are actually very different from the uncompressed image.
For example, JPEG artifacts are "honest" and "annoying", while WebP and HEIC artifacts are "not honest" and "not annoying".
FUIF aims for compression artifacts that are "honest" and "not annoying". At low bitrates, pixelation will become
obvious at a 1:1 scale, but the overall image fidelity will still be as high as possible
(e.g. comparing a downscaled lossy FUIF image to a downscaled original).
_Rationale: this is a matter of preference, but we think that image fidelity is more important than hiding the fact
that lossy compression was used. An image format should not act as an artistic filter that modifies an image more than
necessary. At least that's our opinion._

* FUIF is royalty __free__ and it has a reference implementation that is free and open source software.
_Rationale: we don't want no patent mess, please._

* FUIF is __legacy-friendly__ but not backwards compatible.
Unlike JPEG XT, the FUIF bitstream is completely different from legacy JPEG, so it is not backwards compatible
(cannot be read by existing JPEG decoders). But unlike most other recent image formats (e.g. WebP, HEIC, AVIF),
existing JPEG images can be converted losslessly and effectively to FUIF,
potentially with added information like alpha or depth channels.
_Rationale: backwards compatibility is obviously convenient, but it has serious downsides.
It implies that the compression density cannot be improved compared to legacy JPEG.
It also means that none of the new features (like e.g. alpha) are guaranteed to be rendered correctly,
since (at least initially) most decoders will ignore any extensions of the legacy JPEG bitstream.
So for these reasons, not being backwards compatible is a better choice.
However it is nice to be able to transcode existing JPEG images to FUIF without generation loss and while saving bytes._


## TL;DR feature summary

* Lossless and lossy, works well for photographic and non-photographic
* Arbitrary number of channels (grayscale, RGB, CMYK, RGBA, CMYKA, CMYKA+depth, etc)
* Bit depth up to 28-bit, arbitrary actual ranges of pixel values
* Progressive/Responsive by design: single-file responsive images
* Minimal header overhead, so LQIP in say a 200-byte budget is possible
* Backwards compatible with JPEG (can losslessly and efficiently represent legacy JPEG files)
* Adjustable trade-off between encode/decode complexity and compression density
