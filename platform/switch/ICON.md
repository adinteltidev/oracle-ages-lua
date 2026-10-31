# Switch homebrew icon

The current NRO embeds `icon.jpg`: a 256 × 256 RGB JPEG containing the simplified
Oracle of Ages logo and the lowercase label **recomp**. `icon-recomp.png` is the
larger edited artwork used to export the JPEG. `icon.svg` is the previous clock
artwork; it is not used by the current NRO build.

Original logo: [The Legend of Zelda Oracle of Ages.png, Wikimedia Commons](https://commons.wikimedia.org/wiki/File:The_Legend_of_Zelda_Oracle_of_Ages.png),
922 × 529, author listed as **Kigsz**. The file page labels the simplified logo
`PD-textlogo` and identifies trademark considerations. This attribution does not
relicense the Zelda branding under the project's source-code license.

The requested text and icon composition were produced using the built-in
imagegen editor on 2026-10-30, with that PNG as the edit target. The JPEG was
exported using macOS `sips`, without additional content edits.

Final editing prompt:

> Use case: precise-object-edit. Edit the provided 922x529 Wikimedia Commons simplified Oracle of Ages PNG as the exact logo source for a Nintendo Switch homebrew icon. Create a flat square 1:1 composition, preferably 1024x1024 for clean downscaling to 256x256. Preserve the complete original logo faithfully: the exact THE LEGEND OF / ZELDA / ORACLE OF / AGES wording, original letterforms, purple fills, white and turquoise outlines, proportions and arrangement. Do not redesign the logo or add artwork or symbols. Place it centered and large in the upper two thirds, with comfortable padding and no cropping. Use a solid very dark blue-violet background harmonious with the original purple/turquoise palette, fully opaque, no gradients or mockup. Below the original logo, in its own clear space, add exactly the lowercase text "recomp" (r e c o m p), centered in a bold simple sans-serif in white, large and readable when the whole square is reduced to 256 pixels. This is the only new text. Keep the original logo and the recomp line separate; do not replace AGES. Clean sharp edges, no extra frame, no Switch branding, no characters, no 3D or perspective, no other text.

Export the icon again with:

```sh
sips -s format jpeg -s formatOptions best -z 256 256 \
  platform/switch/icon-recomp.png --out platform/switch/icon.jpg
```

`platform/switch/targets.cmake` passes this file to `elf2nro --icon` and tracks it
as a build dependency. Rebuilding `switch-nro` incorporates the changed JPEG.
