# What is in dgx_ui_preview.js, and under which licences

The module in `dgx_ui_preview.js` is compiled from:

| What | Licence | File |
|---|---|---|
| dgx-ui's sources and the preview's own | MIT | `LICENSE` at the top of dgx-ui |
| DGX, its sources for a computer (the version in the head of `dgx_ui_preview.js`) | MIT | `LICENSE-DGX` |
| the fonts TerminusTTFMedium12 and LiberationSansRegular12 of DGX | SIL Open Font License 1.1 | `OFL-1.1.txt` |
| the font DejaVuSansBook32 of DGX | the Bitstream Vera licence, the DejaVu changes in the public domain | `DejaVu-LICENSE.txt` |
| the font DroidSansRegular9 of DGX | Apache License 2.0 | `Apache-2.0.txt` |
| the font font0ss of DGX | MIT, as DGX | `LICENSE-DGX` |
| the objects of wasi-libc, the C library, that `taken.txt` lists | by Ubuntu's package: CC0-1.0, MIT (musl), BSD-2-Clause (cloudlibc), Apache-2.0 (WASI's headers) | `wasi-libc.copyright` |
| the object of compiler-rt's builtins that `taken.txt` lists | Apache-2.0 WITH LLVM-exception | `libclang-rt-21-dev-wasm32.copyright` |

`FONTS-DGX.md` is DGX's table of its fonts, where each of them came from. `taken.txt` is what the link map says the
module took of the C library and of the compiler's builtins, with the versions of the packages. Every file here but
this one is made by `../Makefile`, from the copy of DGX and the packages the module is built with, and held by
`make check`.
