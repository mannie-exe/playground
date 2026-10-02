# Bundled theme fonts

Unmodified static TTF faces from pinned upstream desktop releases. No system font
installation or font download is required at build or runtime. CMake installs this
directory, including licenses, with the application assets.

| Family | Version / source | License |
| --- | --- | --- |
| Inter and Inter Display | [4.1](https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip) | [OFL 1.1](Inter/OFL.txt) |
| JetBrains Mono | [2.304](https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip) | [OFL 1.1](JetBrainsMono/OFL.txt) |
| Source Serif 4 | [4.005](https://github.com/adobe-fonts/source-serif/releases/download/4.005R/source-serif-4.005_Desktop.zip) | [OFL 1.1](SourceSerif4/OFL.txt) |
| Noto Color Emoji | [e20cbc2](https://github.com/googlefonts/noto-emoji/tree/e20cbc2bbec1926686be9f9bee7d1d2cfa1fea0e) | [OFL 1.1](NotoEmoji/OFL.txt) |

Inter and Inter Display include weights 100–900; JetBrains Mono includes 100–800.
Source Serif 4 includes 200, 300, 400, 600, 700 and 900 in Text, Caption, SmallText,
Subhead and Display optical designs. Every weight includes upright and italic
faces: 112 faces total. JetBrains Mono is the original family, without Nerd Font
patches. Variable and webfont copies are omitted.

app::themeFontAssets() registers every face by stable logical ID, family, weight,
slant and optical design. Registration does not open fonts. Theme selection opens
only requested faces through the existing resource cache.

NotoEmoji/NotoColorEmoji.ttf is the unmodified 2D/fonts/NotoColorEmoji.ttf from
the pinned Noto revision, registered as app.font.noto-emoji. Its SHA-256 is
`15671215ab769fdc7162a045d56fd7d7e477c51b04e6b3c761d914d8fdd6cc44`.
On macOS the installed Apple Color Emoji precedes Noto in the fallback chain;
it is a system resource and is not bundled. See Apple's
[macOS license, section 2E](https://www.apple.com/legal/sla/docs/macOSTahoe.pdf).

## Archive checksums

SHA-256 of the downloaded archives:

- Inter and Inter Display 4.1: `9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e`
- JetBrains Mono 2.304: `6f6376c6ed2960ea8a963cd7387ec9d76e3f629125bc33d1fdcd7eb7012f7bbf`
- Source Serif 4 4.005: `549fdb8f9a682bd06944298621404969f6de77c2e422ff3b8244a1dcd6a0c425`

LBRITE.TTF and the Jurriaan files predate these theme assets; they are not part of
the new default font families.
