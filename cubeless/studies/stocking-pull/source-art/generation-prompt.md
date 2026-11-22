# Fine Fishnet Mask Source

Generated with the built-in image_gen tool (not the CLI/API fallback). Selected PNG is `T_FishnetFine_Opacity_V1.png`, copied unmodified into this directory. The model returned 1254x1254 RGB; the requested 1024 size/cell count is prompt intent, not a measured output count. UE uses StretchToPowerOfTwo for mip generation.

The image is a grayscale R-channel opacity mask. Black holes and white threads control a Translucent material; PNG alpha is not required. A first transparent-image attempt had artifacts in the holes and was rejected. Its original remains in the generated_images directory.

## Exact final generation prompt

```text
Use case: stylized-concept. Asset type: grayscale seamless tileable game fabric opacity MASK. Make a perfectly clean flat two-color technical texture, square 1024x1024. PURE SOLID BLACK background (RGB 0,0,0) with a regular fine WHITE diamond lattice (RGB 255,255,255), like delicate fine fishnet stocking threads. Approximately 16 small diamond openings across the square and 16 vertically. Very thin consistent white lines, about 3 pixels wide, smooth anti-aliased edges, tiny simple intersections. Nothing at all inside the black openings: they must be completely flat, spotless solid black. Perfectly regular repeating geometry, exact spacing throughout, continuous thread phase at all four tile borders. The entire image is just this fine white-on-black diagonal diamond net mask, extending edge to edge. No realistic fabric noise, no fibre texture, no speckles, no grain, no shadow, no highlights, no gradients, no folds, no perspective, no photograph, no garment, no body or legs, no checkerboard, no transparency, no border, no text. This black-and-white image is sampled as a material opacity mask, not used as a final colored illustration.
```
