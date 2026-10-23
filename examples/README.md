# Examples and website screenshots

- [`fnk0104s-esp-idf.yaml`](fnk0104s-esp-idf.yaml): full reference device with
  display, visible image selection, static logos, and an image-scaling demo.
- [`fnk0104s-hidden-images.yaml`](fnk0104s-hidden-images.yaml): mixed visible,
  hidden, and static image controls.
- [`secrets.example.yaml`](secrets.example.yaml): private Wi-Fi/API-key template.
- [`assets/sd-image-placeholder.svg`](assets/sd-image-placeholder.svg): required
  compile-time placeholder, not an SD photograph.

The GitHub Pages site documents these existing configurations and does not
change their behavior. Its installation and configuration fragments are
explained in [`docs/WEBSITE.md`](../docs/WEBSITE.md). Copy the full device YAML
and its `assets/` folder when starting a device; snippets on the website that
are marked as fragments are not standalone device configurations.

The example screenshots and photos are owned and/or created by the maintainer.
Any AI-assisted work was created under the maintainer's direction. The site
reuses these assets without editing their pixels:

| File | Content | Owner / creator |
|---|---|---|
| `en-dark-1040.png` | English desktop web file manager, 1040 × 1199 | Maintainer; any AI assistance under the maintainer's direction |
| `de-dark-375.png` | German mobile web file manager, 375 × 1630 | Maintainer; any AI assistance under the maintainer's direction |
| `HA-Example1.png` | Home Assistant dashboard example, 876 × 556 | Maintainer; any AI assistance under the maintainer's direction |
| `ESPHome-SDWebAndImageServer.jpg` | Actual device photograph with a custom six-tile dashboard, 1980 × 1493 | Maintainer; any AI-assisted visual work under the maintainer's direction |

German labels and user filenames inside the original images are retained and
identified in the site's English captions. The HA screenshot includes controls
from the maintainer's wider dashboard; these are not all provided by this
component. The hardware photo shows a custom dashboard; its artwork, sensor
data, and full layout are not supplied by the SD server component. Website
captions explain this distinction.

The source photograph and both generated website copies have been stripped of
JPEG metadata. The website build strips all JPEG application segments, comments,
and trailing data while preserving encoded image data. The checker also rejects
metadata in the source photo, so future replacements must be sanitized too.
