#!/bin/bash
# Signs SumatraPDF.app with a Developer ID certificate and has Apple notarize
# it, so macOS opens it without "Open Anyway". The workflow runs this only
# when the repository has the secrets:
#   MACOS_CERT_P12       the "Developer ID Application" certificate and its
#                        key, exported as .p12 and base64-encoded
#   MACOS_CERT_PASSWORD  the .p12's password
#   APPLE_ID             the Apple Account of the developer team
#   APPLE_TEAM_ID        the team's ID (developer.apple.com > Membership)
#   APPLE_APP_PASSWORD   an app-specific password of that account
#                        (account.apple.com > Sign-In and Security)
# Usage: sign-mac.sh <SumatraPDF.app> sign | notarize | sign-dmg <dmg>
set -euo pipefail
app=$1
step=$2

identity() {
  security find-identity -v -p codesigning | grep "Developer ID Application" | head -1 | awk '{print $2}'
}

# the certificate goes into a keychain of its own, which codesign may use
# without a prompt
import_cert() {
  local keychain=$RUNNER_TEMP/sign.keychain-db
  local pass
  pass=$(uuidgen)
  echo "$MACOS_CERT_P12" | base64 --decode >"$RUNNER_TEMP/cert.p12"
  security create-keychain -p "$pass" "$keychain"
  security set-keychain-settings -lut 21600 "$keychain"
  security unlock-keychain -p "$pass" "$keychain"
  security import "$RUNNER_TEMP/cert.p12" -k "$keychain" -P "$MACOS_CERT_PASSWORD" -T /usr/bin/codesign
  security set-key-partition-list -S apple-tool:,apple: -s -k "$pass" "$keychain" >/dev/null
  security list-keychains -d user -s "$keychain" $(security list-keychains -d user | tr -d '"')
  rm "$RUNNER_TEMP/cert.p12"
}

# inside out, with the hardened runtime notarization requires: the
# frameworks, the app extensions with the entitlements the build gave them,
# then the app
sign_app() {
  import_cert
  local id
  id=$(identity)
  if [ -z "$id" ]; then
    echo "no Developer ID Application identity in MACOS_CERT_P12"
    exit 1
  fi
  local sign=(codesign --force --timestamp --options runtime --sign "$id")
  for f in "$app"/Contents/Frameworks/*; do
    "${sign[@]}" "$f"
  done
  for x in "$app"/Contents/PlugIns/*.appex; do
    codesign -d --entitlements - --xml "$x" >"$RUNNER_TEMP/appex.entitlements"
    "${sign[@]}" --entitlements "$RUNNER_TEMP/appex.entitlements" "$x"
  done
  "${sign[@]}" "$app"
  codesign --verify --deep --strict --verbose=2 "$app"
}

# Apple checks the app; the ticket is stapled to it, so Gatekeeper needs no
# network the first time it opens
notarize_app() {
  local zip=$RUNNER_TEMP/SumatraPDF.zip
  ditto -c -k --keepParent "$app" "$zip"
  xcrun notarytool submit "$zip" --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" \
    --password "$APPLE_APP_PASSWORD" --wait
  xcrun stapler staple "$app"
  spctl --assess --type execute --verbose "$app"
  rm "$zip"
}

case "$step" in
  sign) sign_app ;;
  notarize) notarize_app ;;
  sign-dmg) codesign --force --timestamp --sign "$(identity)" "$3" ;;
  *)
    echo "unknown step '$step'"
    exit 1
    ;;
esac
