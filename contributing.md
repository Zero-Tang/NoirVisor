# Contribution Guidelines
As the owner of the open-source project, I welcome and encourage the community to contribute to project NoirVisor. This document is intended to standardize the rules on contribution.

## Pull-Requests
To contribute to NoirVisor, you need to make a fork of this repository to your own account. Submit changes to your forked repository. When your patch completes, submit a [PR (short for Pull Request)](https://github.com/Zero-Tang/NoirVisor/pulls).

If your patch is trivial, there isn't any specific commit history requirement. The PR will merge to `master` branch with the **Squash** strategy. You need to "force" a synchronization between the fork after the merge via **rebase**.

If your patch is substantial, you need to maintain a strict commit history. This PR will merge to `master` branch with the **Merge-Commit** strategy. The rules for commit history are:

- Changes in each commit belongs to the same purpose. No all-in-one commit.
- No "nit" commits. You need to amend or squash your commits. Use `git rebase -i <commit-hash>` to do so.
- To synchronize new changes from `master` branch to your fork, use the **Rebase** strategy.

Your patch must pass all checks enforced by the GitHub Action runners (in other words, CI checks).

### Statistics Gaming
We welcome meaningful contributions, but we strictly reject pull requests that exist solely to inflate GitHub contribution statistics (a.k.a. Green-Dot Farming). \
The following types of PRs will be closed immediately without review and may result in a user block:
- **Trivial Formatting and Whitespace**: PRs that only fix arbitrary whitespace or that add/remove trailing spaces.
- **Grammar and Typo**: PRs that only fix grammars and typos.
- **Automated or AI-Generated Spam**: See [LLM-usage Guidelines](#llm-usage-guidelines).
- **No-op Code Changes**: Rearranging imports, renaming local variables to subjective alternatives (e.g.: changing `i` to `index` in a micro loop), or adding obvious comments. (e.g.: `// Increments i by 1.`)

## Coding Conventions
For Rust codes, consult [NoirVisor's coding style in Rust](doc/rust.md#coding-style). \
For C codes, you may follow the platform-specific coding style.

In future, NoirVisor may deploy experimental `.clang-format` and `rustfmt.toml` files.

## LLM-usage Guidelines
Using LLMs while working on NoirVisor is conditionally allowed, when done with care. \
**Rule of thumb**: LLMs are not substitutes for thinking and reasoning processes. It is not allowed to be used in the ways that risk losing technical understanding of the software engineering and system architectures, including the standard operating procedures, the computer architectures, the hypervisors and the operating systems.

The guidelines may subject to frequent changes as the LLM technology evolves. However, the **rule of thumb** mentioned above should never be changed.

### Allowed
- You may ask questions about the existing codebase **privately**.
- You may ask LLMs to **privately** assist in debugging.
- You may ask LLMs to **privately** review the codes and documentations.
- You may create a `scratch` folder, which is explicitly ignored via `.gitignore`, and put whatever stuff that LLMs may need to use.
- You may submit LLM-generated codes in your PR. Doing so won't get you discriminated.

### Banned
- Comments that are generated with LLM. Craft your comments in your own words.
	- This includes Issue body and PR descriptions. It is not about comments in the source files.
	- LLM's generated content is not a substitute of your own words.
	- Machine-translated contents that are not explicitly marked. This includes both traditional machine-translators (e.g.: [Google Translate](https://translate.google.com/), [Baidu Fanyi](https://fanyi.baidu.com/), etc.) and asking LLMs to translate texts for you. \
	Machine-translated contents must be posted alongside with your original version.
- Treating LLM's review as a sufficient condition to merge or reject a change.
- Generating or Automating spams with LLM.
- **Violation of the Code of Conduct**: Harassment on any contributors for using LLMs. This includes playing detective for whether someone has used LLM.

### Penalties
Violations will first result in a warning. Repeated violations may result in a ban.

## Raising Issues
Before you start raising the issue, search over the issue list. If there is already someone raised the issue, you may give additional comments. \
For faster resolution, please give your opinion, or give partial code, on resolution as you raise the issue. \
If you have no idea why issue occurs, just state down the phenomena you see detailedly. Plus, state down how to retrigger the problem.

## Language Selection
Developers in all around the world speak different languages. Hence, language used in contributing is significant. From my perspective, I suggest developers to use such language that their intention will be described most accurately. \
However, I am not able to speak all languages around the world. So, I should define following guidelines of language selection. \
It is recommended to use following languages:
- Simplified Chinese, PRC (简体汉语，中华人民共和国)
- English, US

It could also be fine if it is stated in following languages:
- Traditional Chinese, Taiwan Province (正體中文，臺灣省)
- Traditional Chinese, Hong Kong S.A.R (繁體中文，香港特別行政區)
- English, UK

Other written forms of Chinese (e.g Simplified Chinese, Singapore; 简体中文，新加坡) or English (e.g English, Canada), meaning not confusing, are fine as well. \
For example, if you can describe an issue more accurately in Chinese than English, use Chinese even if you can speak English, despite the widely use of English in GitHub.

If you are neither English nor Chinese speakers, and if you have trouble using English to the extent that you require machine-translations, post both the original text and the translated text. The translated text must be in English.

If you wish to contribute to Project NoirVisor, however, all comments in the changes of codes must be in English. No other languages are allowed.