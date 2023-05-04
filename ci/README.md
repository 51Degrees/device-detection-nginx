# API Specific CI Approach

For the general CI Approach, see [common-ci](https://github.com/51degrees/common-ci).

### Differences
- There are no packages produced by this repository, so the only output from the `Nightly Publish Main` workflow is a new tag and release.
- The package update step does not update dependencies from a package manager in the same way as other repos. Instead it checks the supported versions on the [NGINX Plus Releases](https://docs.nginx.com/nginx/releases/) page, and uses that to update `options.json` to ensure they are tested.
- As Windows is not supported, the scripts here can be a little less strict with implementation. e.g. using `cp` rather than `Copy-Item` as we know we're in a Linux environment.

### Build Options

The additional build options for this repo are:
| Option | Type | Default | Purpose |
| ------ | --------- | ---- | ------- |
| `NginxVersion` | string | `1.21.3` | The version of the opensource NGINX code to use for building and testing. |
| `MemCheck` | bool | `false` | If true the binaries are built in Debug, with memory checks enabled. |
| `BuildMethod` | string | `dynamic` | Can be either `static` or `dynamic` to determine how the nginx module is built |
| `FullTests` | bool | `false` | If true, then the full NGINX test suite is run. This should be set to true for the latest NGINX Plus version. |
