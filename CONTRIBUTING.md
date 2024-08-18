# Contributing to SBEMU

First of all, thanks for willing to contribute to this project!

When contributing any code, please mind the following:

- Your code needs to be licensed either under the
[GNU General Public License, version 2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html) (**without** the "or
later" clause), or a license that is compatible with that, such as
[the MIT License](https://opensource.org/license/MIT). If you do not own the copyright to all of the code that you are
contributing, you must verify that all of the code you are contributing is already covered by such a compatible license.
- You are encouraged to add an [SPDX](https://spdx.org) license identifier to the source files that you add to the
project. If you do so, please add the identifier in a comment at the top of the file.
- Please make sure that any Git commit messages follow the [Conventional Commits](https://www.conventionalcommits.org)
specification. This is required for any Pull Requests you submit, and this is enforced in a CI/CD check. So commit
message for new features must be prefixed with `feat: `, commit messages for bug fixes must be prefixed with `fix: `,
etc. This will help with the automatic generation of a changelog and release notes.
- To keep your branch up-to-date, please **rebase** it on the upstream `main` branch. Please don't merge any changes in
`main` into your branch.
- If you add any new functionality to SBEMU, such as a new driver that supports additional sound devices, please also
add this info to the `README.md` file of this project.

These contributing guidelines will likely be expanded and changed over time, so please check this file for any updates,
before you submit any Pull Requests.

Thank you for your cooperation! 🙏🏽
