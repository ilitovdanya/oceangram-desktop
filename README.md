# OceanGram for Windows

Production endpoint: `2.26.197.16:2398` (DC 2). API layer: **232**.

The `og13` branch builds Telegram Desktop 7.3.3 with the existing OceanGram features and server RSA keys.
Run Actions -> **Build OceanGram Desktop** on branch `og13`. The APK/Windows installer, portable build, and `oceangram-build.json` manifest are attached to the successful run.

Sources are pinned to immutable upstream commits in the workflow. `patch/verify_build.py` checks the schema layer and actual bootstrap endpoint before compilation. Android releases keep the existing signing key so they can update the installed OceanGram app.

The libraries job caches Qt/WebRTC/FFmpeg/Rust build stages. If its time limit is reached, rerun the workflow to continue from the saved stages.
