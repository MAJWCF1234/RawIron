RawIron — split full-workspace archive (GitHub release)

Files:
  RawIron_full_release_with_builds.zip.part01
  RawIron_full_release_with_builds.zip.part02
  RawIron_full_release_with_builds.zip.part03
  RawIron_full_release_with_builds.zip.part04

Join (concatenate in order 01 + 02 + 03 + 04) into:
  RawIron_full_release_with_builds.zip

SHA256 of the joined ZIP (verify before extracting):
  94541f13d44e2e8bc247d7dfef87c248b42565670343eadd203277488fa6bd79

PowerShell:
  Get-FileHash -Algorithm SHA256 .\RawIron_full_release_with_builds.zip

Then extract the ZIP to your chosen folder. The tree includes sources and build\dev-msvc (RelWithDebInfo).
Alternatively run Installer\RawIron.FullWorkspace.Installer.cmd from a repo checkout (it downloads these parts from GitHub).
