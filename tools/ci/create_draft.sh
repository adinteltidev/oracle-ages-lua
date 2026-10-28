#!/bin/bash
set -euo pipefail
: "${RELEASE_TAG:?Release tag required}"
: "${GITHUB_SHA:?Source commit required}"
: "${RUNNER_TEMP:?Temporary directory required}"
: "${GITHUB_REPOSITORY:?Repository required}"
if gh release view "$RELEASE_TAG" --repo "$GITHUB_REPOSITORY" > /dev/null 2>&1; then
  echo 'Release already exists; assets will not be replaced.' >&2
  exit 1
fi
notes="$RUNNER_TEMP/oracle-release-notes.md"
mv release-assets/RELEASE-NOTES.md "$notes"
args=(--draft --latest=false --target "$GITHUB_SHA" --title "Oracle Ages Native $RELEASE_TAG" --notes-file "$notes")
if [ "${PRERELEASE:-false}" = true ]; then args+=(--prerelease); fi
if [ "${EVENT_NAME:-}" = push ]; then args+=(--verify-tag); fi
gh release create "$RELEASE_TAG" --repo "$GITHUB_REPOSITORY" release-assets/* "${args[@]}"
