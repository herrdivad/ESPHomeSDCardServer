# Website dependencies and publication considerations

Inventory updated on 2026-10-23. Scope: the static Pages website, its build
scripts, and deployment workflow. ESPHome firmware dependencies and the optional
host-side image converter are separate and are not bundled into the website.

## Code, packages, and services

| Item | Current use | License / publication relevance |
|---|---|---|
| HTML and CSS | Project-specific source in `website/` | Repository MIT license; retain its notice when redistributing code |
| Browser JavaScript | One local file, `website/assets/site.js`; written for this site | Project source under the repository license; no third-party JS library bundled |
| JS behavior | Mobile menu, screenshot/photo dialog, and user-triggered code copying | Clipboard writes occur only after pressing Copy; the script never reads the clipboard |
| JavaScript frameworks / npm packages | None | No React, Vue, jQuery, Bootstrap, lightbox library, or npm installation |
| Fonts | Installed system fonts: Arial, Helvetica, generic sans-serif; Courier New and generic monospace | No font files are distributed or downloaded; users' existing system fonts are used |
| SVG icon / diagram | Project-specific favicon and HTML/CSS architecture diagram | No external icon library or logo asset is bundled |
| Design reference | ChemAssistent's visual and structural direction | No ChemAssistent JavaScript, screenshots, logos, or complete template is bundled |
| Images | Three example screenshots and the real hardware photo | Owned and/or created by the maintainer; any AI-assisted work was created under the maintainer's direction |
| Build | Python 3.10+ standard library | No pip dependencies or static-site generator installation |
| JS syntax check | `node --check`, in CI or optionally on the developer machine | No npm dependencies; Node is tooling, not a visitor download |
| Hosting | GitHub Pages | GitHub's service terms and privacy practices apply; requests necessarily reach GitHub's infrastructure |
| CI/deployment | Official GitHub Actions below | Third-party tooling used on the runner, not browser libraries or website payload |
| Outbound links | GitHub, original project, and the supplied legal notice | Ordinary navigation links; no iframe, tracking widget, social embed, or preload of those targets |

The four top-level workflow actions declare MIT licenses:

- [`actions/checkout@v6`](https://github.com/actions/checkout/blob/v6/LICENSE)
- [`actions/configure-pages@v5`](https://github.com/actions/configure-pages/blob/v5/LICENSE)
- [`actions/upload-pages-artifact@v4`](https://github.com/actions/upload-pages-artifact/blob/v4/LICENSE)
- [`actions/deploy-pages@v4`](https://github.com/actions/deploy-pages/blob/v4/LICENSE)

These actions may have their own transitive dependencies. This inventory does
not claim a full audit of those runner-side dependency trees. Their code is not
copied into the published site. The Pages artifact contains no package manager
lockfile, `node_modules`, Python environment, or firmware binary.

## Browser storage and personal data

The website's own code sets no cookies and uses no `localStorage`,
`sessionStorage`, IndexedDB, service worker, fingerprinting, analytics, ads,
forms, account system, or network API calls. Images, CSS, and JavaScript load
from the same Pages site. Canonical/Open Graph URLs are metadata, not embeds.
Ordinary browser/HTTP caching is not an application tracking mechanism.

The firmware's device-hosted WebUI is different: it uses `sessionStorage` for
the device API key and `localStorage` for language/theme preferences. The Pages
site documents that interface but does not run it, access a device, or collect
device credentials.

GitHub explicitly states that Pages visitors' IP addresses are logged and
stored for security purposes, even when they are not signed in. See
[GitHub Pages: data collection](https://docs.github.com/en/pages/getting-started-with-github-pages/what-is-github-pages#data-collection)
and [GitHub's privacy statement](https://docs.github.com/en/site-policy/privacy-policies/github-general-privacy-statement).
The absence of custom tracking does not mean there is no hosting-side personal
data processing. No hosted response headers were audited because the site has
not been deployed during this work.

Based on the current site code, there is no identified non-essential cookie
or tracking-storage feature requiring a cookie-consent banner. This is a
technical assessment of that code, not a blanket legal exemption. German
[TDDDG section 25](https://www.gesetze-im-internet.de/ttdsg/__25.html) addresses
storage/access on end-user devices and exceptions. Reassess if analytics,
embeds, forms, remote fonts, or additional scripts are added. A privacy notice
and an imprint are separate questions from a cookie banner.

## Legal notice and privacy information

The shared footer and the 404 page link to the owner's supplied
[legal notice](https://david-herrmann.net/impressum.html). A named copyright
holder or GitHub profile link alone is not a substitute for the required
provider information.

For an operator in Germany, [MStV section 18](https://www.gesetze-bayern.de/Content/Document/MStV-18)
requires provider information for telemedia that do not serve exclusively
personal or family purposes. A public software-project presentation should
not be assumed to fall within that narrow exception merely because it is free
or open source. The [media authorities' guidance](https://www.die-medienanstalten.de/aufgaben/aufsicht/impressumspflicht/)
explains the name/address requirements. [DDG section 5](https://www.gesetze-im-internet.de/ddg/__5.html)
can impose additional information duties for the offers within its scope.

The operator should make clear that the linked legal notice covers this Pages
project, keep it accurate and reachable, and assess a privacy notice describing
the actual hosting arrangement and applicable processing. The external link
was supplied by the owner; no address or legal identity has been invented or
copied into project code. This inventory is not an individual legal opinion or
a certification that the final publication satisfies every legal obligation.

## Hardware photograph and metadata

The example screenshots and photographs are owned and/or created by the
maintainer. Any AI-assisted visual work was created under the maintainer's
direction. This attribution records the maintainer's express statement about
the supplied example assets.

`examples/ESPHome-SDWebAndImageServer.jpg` and both generated website copies
have been stripped of all JPEG application segments, comments, and trailing
data. This removes EXIF/GPS, XMP, IPTC, JFIF, ICC, embedded thumbnails, and other
application metadata. Required JPEG coding headers remain. The encoded image
data is preserved without recompression; an independent decode comparison
confirmed identical dimensions and pixels for each of the three files.

The build applies the same cleanup to future generated photo copies, including
metadata between scans and after the JPEG end marker. The checker also rejects
metadata in the source photo. No metadata-bearing backup was created. This
cleanup concerns the actual image files in this workspace; it does not remove
copies in external uploads, browser caches, or previously published sources.

Home Assistant and ESPHome names remain subject to their owners' trademark
rights; the site retains the independent-project disclaimer.
