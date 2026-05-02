RawIron — split full-workspace archive (GitHub release)

Files:
  RawIron_full_release_with_builds.zip.part01
  RawIron_full_release_with_builds.zip.part02
  RawIron_full_release_with_builds.zip.part03

Join (concatenate in order 01 + 02 + 03) into:
  RawIron_full_release_with_builds.zip

SHA256 of the joined ZIP (verify before extracting):
  f3ac837cd2d32f0fbdfdd2469e4c58c2ab67f9decb4915caeeb1bcf33175157c

PowerShell:
  Get-FileHash -Algorithm SHA256 .\RawIron_full_release_with_builds.zip

Then extract the ZIP to your chosen folder. The tree includes sources and build\dev-msvc (RelWithDebInfo).
Alternatively run Installer\RawIron.FullWorkspace.Installer.cmd from a repo checkout (it downloads these parts from GitHub).
