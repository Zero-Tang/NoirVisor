# Contribution Guidelines
As the owner of the open-source project, I welcome and encourage the community to contribute to project NoirVisor. This document is intended to standardize the rules on contribution.

## Programming Language Selection in Contribution
As already announced in the "readme" file in NoirVisor's repository, NoirVisor's owner, Zero Tang, does not welcome languages other than C and Assembly to involve the NoirVisor's core.

## Sign-Off
It is significant that NoirVisor will record contributions and corresponding contributors. Contribution details will be stated as a piece of comment in relevant code area. \
For successful contribution, please write down your name at the bottom that can directly recognize you with following format: \
Netizen Name, [E-Mail Address] \
By saying Netizen Name, it refers to the name you use in GitHub. Your real name can also be accepted. If you prefer the name you use in other forums, you may state down that forum website. \
Starting from your first contribution, you, nonetheless, cannot use other names on your future contribution, unless you contribute through another account.

## Pull-Requests
To contribute, you have to fork this repository, make your patches, then open a Pull-Request. \
Each commit of your patch should be able to be built for all targets. If you pushed your commits without realizing, you may use `git rebase` command to amend them and then do a `git push -f`. For example, you may squash the "fix" commit into its previous commit.

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
**Rule of thumb**: LLMs are not substitutes for thinking and reasoning processes. It is not allowed to be used in the ways that risk losing technical understanding of the system architectures, including the processors, the hypervisors and the operating systems.

The guidelines may subject to frequent changes as the LLM technology evolves. However, the **rule of thumb** mentioned above should never be changed.

### Allowed
- You may ask questions about the existing codebase **privately**.
- You may ask LLMs to **privately** assist in debugging.
- You may ask LLMs to **privately** review the codes and documentations.
- You may create a `scratch` folder, which is explicitly ignored via `.gitignore`, and put whatever stuff that LLMs may need to use.
- You may use LLMs to generate possible solutions of an issue, learning from them, but eventually solve it from scratch on your own.

### Banned
- Comments that are generated with LLM. Craft your comments in your own words.
	- This includes Issue body and PR descriptions.
	- LLM's generated content is not a substitute of your own words.
	- Machine-translated contents that are not explicitly marked. This includes both traditional machine-translators (e.g.: [Google Translate](https://translate.google.com/), [Baidu Fanyi](https://fanyi.baidu.com/), etc.) and asking LLMs to translate texts for you. \
	Machine-translated contents must be posted alongside with your original version.
- Codes and Documentations that are generated with LLM.
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