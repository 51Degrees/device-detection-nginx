# API Specific CI/CD Approach
This API does not produce packages so use `tag-repository` approach as described in `common-ci` `README.md`.

Differences:
- There is no `create-packages` pipeline, but `tag-repository` pipeline.
- `deploy` pipeline only push tag and branch to GitHub.
- `Debug` entries are not required in the strategy matrix for `Build and Test` pipeline since the `Memory Leak` tests require to be run with `Debug` build, so it already covers the `Debug` testing.