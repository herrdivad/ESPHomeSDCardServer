# GitHub Pages website

The project website is a lightweight static landing page and technical guide.
All visible copy, HTML, CSS, JavaScript, and comments are English. Original
repository screenshots retain their original labels and user filenames; their
captions explain this. No screenshot or hardware photograph was fabricated.
The example screenshots and photos are owned and/or created by the maintainer;
any AI-assisted work was created under the maintainer's direction.

The visual reference is [ChemAssistent](https://starmanmartin.github.io/ChemAssistent/):
charcoal sections, large sans-serif headings, blue and yellow accents, generous
spacing, framed screenshots, and clear technical diagrams. This site adapts that
presentation to a responsive scrolling page, rather than a fixed slide deck.
All fonts are system fonts (`Arial`, `Helvetica`, and standard monospace fonts).
There are no downloaded fonts, runtime CDN dependencies, analytics, or package
installation steps. Python 3.10+ is sufficient to build and check the artifact.

## Source and content

- `website/index.html`: landing page, capabilities, actual screenshots, test
  status, limitations, and documentation links.
- `website/guide.html`: installation, configuration, SD images, Home Assistant,
  memory, HTTP API, security, and troubleshooting.
- `website/404.html`: project-subpath-aware fallback, including nested bad URLs.
- `website/partials/`: shared navigation and footer.
- `website/assets/`: CSS, optional progressive-enhancement JavaScript, and the
  original SVG site icon.
- `scripts/build_website.py`: standard-library build; expands shared partials,
  reproduces the minimal YAML and endpoint table from README, and copies only
  named public inputs to `build/pages/`.
- `scripts/check_website.py`: offline validation of the artifact and internal links.

The screenshot sources are `examples/en-dark-1040.png`,
`examples/de-dark-375.png`, and `examples/HA-Example1.png`, owned and/or created
by the maintainer, with any AI assistance under the maintainer's direction.
The build copies these byte for byte; edits to the component's embedded WebUI
are not required.

The real hardware photograph is `examples/ESPHome-SDWebAndImageServer.jpg`.
It is owned and/or created by the maintainer, with any AI-assisted visual work
created under the maintainer's direction. English captions identify the dashboard as a
maintainer setup rather than a bundled component feature. The source and all
three known project photo files (source plus two generated copies) have been
stripped of JPEG metadata. The build removes all JPEG application segments
(including EXIF/GPS, XMP, IPTC, JFIF, and ICC), comments, and trailing data
without recompressing or changing encoded pixels. Required JPEG coding headers
remain. Source and public copies are checked for metadata on every build check.

See [Website dependencies and publication considerations](WEBSITE_DEPENDENCIES.md)
for the current JavaScript, package, license, storage, and hosting inventory.
The shared footer and 404 page link to the owner's supplied
[legal notice](https://david-herrmann.net/impressum.html). This is an ordinary
outbound link, not an embedded page or a request made when the project page loads.

Use README and actual component implementation as the source of truth. Keep the
landing page and guide synchronized when behavior changes. Do not present
historical plans as current features: hidden Selects, fit modes, and PSRAM
diagnostics are implemented; runtime directory/wildcard image discovery, an HA
image-library refresh button, and MP3 playback remain planned. Hardware results
are maintainer reports, and detailed open test scenarios remain open. No fresh
hardware verification is implied by website checks.

## Build and validate locally

Run from the repository root:

```bash
python3 scripts/build_website.py
python3 scripts/check_website.py
node --check website/assets/site.js
python3 -m unittest discover -s tests -p 'test_website_metadata.py' -v
```

Node is only needed for the optional JavaScript syntax check. No npm packages
are used. The generated artifact is ignored by Git under the existing `build/`
rule. Build output is restricted to a subdirectory of `build/`; the checker
rejects unexpected artifact files and symlinks. The build never copies the
checkout, secrets, local patch manifest, firmware, or development plans wholesale.

To preview the exact GitHub project path:

```bash
python3 scripts/build_website.py --output build/preview/ESPHomeSDCardServer
python3 scripts/check_website.py --output build/preview/ESPHomeSDCardServer
python3 -m http.server 8000 --bind 127.0.0.1 --directory build/preview
```

Open `http://127.0.0.1:8000/ESPHomeSDCardServer/`. Stop the server with Ctrl+C.
Test both pages at desktop and mobile widths, screenshot expansion (including
Escape and focus return), mobile navigation, code copying, and native expandable
sections. Content and links must also remain usable with JavaScript disabled.

The checker validates local paths and fragments under `/ESPHomeSDCardServer/`,
repository file and Markdown-heading destinations against the local checkout,
image alt text, metadata, unique IDs, ARIA targets, unchanged screenshot bytes,
the hardware photo's metadata-sanitized copy,
and the absence of external runtime dependencies. It does not make network
requests or promise the availability of third-party links, and does not replace
browser accessibility or visual inspection.

## Deployment after human approval

No publication, remote settings change, or Git write is performed by preparing
these files. A human must approve, commit, and push the patch. Before that push,
the repository owner should select **Settings → Pages → Build and deployment →
Source: GitHub Actions**. The workflow follows GitHub's
[custom Pages workflow requirements](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages).

`.github/workflows/pages.yml` builds and validates on pushes and pull requests
targeting `master`. Pull requests run checks only. After the approved patch is
pushed to `master`, the workflow automatically uploads the artifact and deploys
it using the `github-pages` environment. A manual dispatch from `master` is also
supported. Deployment needs `pages: write` and `id-token: write`; checkout uses
read-only contents permission and does not persist credentials.

The expected public URL is
`https://herrdivad.github.io/ESPHomeSDCardServer/`. Ordinary links and assets are
relative to the page directory. The canonical URLs and 404 fallback explicitly
use `/ESPHomeSDCardServer/`. Update those values and the checker's `BASE` and
`REPOSITORY` constants if hosting a fork under another repository name.
