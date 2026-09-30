# OTA signing for crosspoint-x-reader

This fork verifies OTA updates against an **Ed25519 signature** so a tampered or
mismatched firmware image is rejected before it is flashed. The firmware image
itself is **not** co-signed/locked — only a small per-release **manifest** is
signed, and the streamed firmware is checked against the manifest's signed
SHA-256. This is the standard *sign the digest, stream-verify the bytes* pattern.

## How it works

1. On a supported tag, `.github/workflows/release.yml` builds both
   `crosspoint-<version>-<device>.bin` and `xpoint-<version>-<device>.bin`
   for each board, then generates `manifest.json`:

   ```json
   {
     "version": "1.2.3",
     "repository": "OWNER/REPO",
     "boards": [
       { "board": "x4pro",
         "url": "https://github.com/OWNER/REPO/releases/download/v1.2.3/crosspoint-1.2.3-x4pro.bin",
         "size": 1234567, "sha256": "<64 hex>" }
     ]
   }
   ```

2. The workflow checks that `OTA_SIGNING_KEY` is a base64-encoded 32-byte
   Ed25519 seed matching the public key compiled into the firmware. It signs
   the **exact manifest bytes** (including the final newline), then adds
   `manifest.json` and `manifest.json.sig` (base64-encoded signature) to a
   **draft** GitHub release alongside the firmware assets.

   If the secret is missing, malformed, or does not match, the job fails before
   release creation or update. An existing draft stays unchanged. The workflow
   never emits an unsigned replacement manifest; a human must review and
   publish a successfully signed draft.

3. On device, `OtaUpdater` (in `src/network/OtaUpdater.cpp`):
   - rejects a release with no signed manifest,
   - fetches `manifest.json` + `manifest.json.sig` from the release,
   - verifies the signature with the **public key baked into**
     `lib/OtaSignature/ota_pubkey.h` (`ota_signature::PUBKEY`),
   - pins the running board's entry (URL, size, SHA-256) and enables the update
     only after successful verification,
   - streams the firmware and computes its SHA-256, comparing it to the signed
     hash before marking the OTA partition bootable; `installUpdate()` also
     refuses to run without a pinned SHA-256.
   - The existing chip-id + embedded board-tag guards still run as defense in depth.

Releases without a signed manifest, including older or third-party releases,
cannot be installed through OTA. A missing manifest, invalid signature, or SHA
mismatch fails the update with `SIGNATURE_ERROR`. Missing or unreachable
manifest files can also produce `HTTP_ERROR`.

## Keys

- **Private key** — `OTA_SIGNING_KEY` repo secret (raw 32-byte Ed25519 seed, base64).
  Never committed. Used only by the release workflow.
- **Public key** — `lib/OtaSignature/ota_pubkey.h`, committed. Anyone can read it;
  only someone with the secret can produce a valid signature.

## Triggering a release (easy OTA workflow)

1. Bump `crosspoint.version` in `platformio.ini`.
2. Commit + push to `develop`.
3. Tag and push:

   ```bash
   git tag v1.2.3 -m "Release v1.2.3"
   git push origin v1.2.3
   ```

4. Check that the workflow succeeded and the **draft** contains both firmware
   asset families, `manifest.json`, and `manifest.json.sig`. Publish the draft
   after review. A missing or invalid `OTA_SIGNING_KEY` leaves no new release;
   an existing draft is untouched.

## Rotating the signing key

The device trusts exactly one baked-in public key (`ota_signature::PUBKEY` in
`lib/OtaSignature/ota_pubkey.h`). To rotate without bricking OTA for devices
that haven't updated yet, you must ship the new key in a *transitional* firmware
before any release is signed with it:

1. **Generate a new Ed25519 keypair** (raw 32-byte seed) and keep the old one
   available for one more release cycle.

2. **Update the firmware to trust BOTH keys.** Extend `verifyManifest()` to try
   each key in a small built-in list (`PUBKEY_PRIMARY`, `PUBKEY_PREVIOUS`) and
   accept if either verifies. The release workflow currently checks the signing
   seed against the single `PUBKEY` in `ota_pubkey.h`; update that check in the
   same change so it accepts the intended transitional key without accepting
   an unrelated one. Commit the dual-key firmware and matching workflow.

3. **Cut and publish a release signed with the OLD key** (so it is still
   authentic to every device in the field — including those running firmware that
   only knows the old key). This propagates the dual-key firmware from step 2
   to the fleet. Wait for broad adoption before continuing.

4. **Switch the signing secret to the NEW key** and cut the next release. The
   dual-key firmware accepts it. Devices still on the old single-key firmware
   that missed the step-3 rollout will now reject the new-key release and keep
   their current firmware (safe, no brick) until they install the transitional
   firmware from step 3.

5. **Later**, once the old single-key fleet is negligible, drop
   `PUBKEY_PREVIOUS` from the firmware in a normal release and restore the
   workflow's single-key check.

```bash
# Generate a new keypair and print the base64 seed (store the seed as OTA_SIGNING_KEY):
python3 - <<'PY'
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
import base64
sk = Ed25519PrivateKey.generate()
print("seed (set as OTA_SIGNING_KEY):", base64.b64encode(sk.private_bytes_raw()).decode())
PY
```
