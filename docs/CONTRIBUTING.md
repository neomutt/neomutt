# Contributing to NeoMutt

Thank you for your interest in contributing to NeoMutt! We appreciate your help in making this project better.

> [!NOTE]
>
> NeoMutt is maintained by a dedicated team of volunteers in their free time. We kindly ask that you take a few minutes to read this document before contributing. Your understanding helps ensure smooth collaboration.

## AI Assistance Notice

If you use any AI assistance while contributing to NeoMutt, **you must disclose this in your pull request**, including the extent to which AI was used (e.g., documentation only vs. code generation). If PR comments are AI-generated, disclose that as well.

An example disclosure:

> This PR was written primarily by Claude Code.

Or a more detailed disclosure:

> I consulted ChatGPT to understand the codebase but the solution was fully authored manually by myself.

Failure to disclose AI assistance is disrespectful to the human maintainers reviewing your pull request, and it makes it difficult to determine the appropriate level of scrutiny to apply to the contribution.

In a perfect world, AI assistance would produce work of equal or higher quality than any human. That isn't our current reality, and in most cases, AI generates subpar results.

When using AI assistance, we expect you to fully understand the code produced and be able to answer critical questions about it. Maintainers are not responsible for reviewing PRs that are so flawed they require significant rework to be acceptable.

Please be respectful to maintainers and disclose AI assistance.

## Reporting Bugs

We have a [bug report template](https://github.com/neomutt/neomutt/blob/main/.github/ISSUE_TEMPLATE/bug-report.md) that guides you through providing the necessary information. Please follow it when opening an issue to ensure we have all the details needed to help you.

To help us help you, please provide as much detail as possible:

- Provide a minimal working example. Try to replicate your issue with a minimal configuration derived from your NeoMutt config and any other relevant config files.
- Be very careful when documenting the steps to reproduce your problem—even small details can matter.
- Include the versions of **all** programs relevant to your report.
- Use Markdown formatting, especially for backtraces and config files.

## Feature Requests

We have a [feature request template](https://github.com/neomutt/neomutt/blob/main/.github/ISSUE_TEMPLATE/feature-request.md) that helps structure your proposal. Please follow it when opening a feature request.

When requesting a feature:

- Describe your problem and its context in detail. Often, the problem can be solved without implementing a new feature.
- Carefully describe your desired solution. Nothing is more frustrating, for both you and the developers, than when we misunderstand your problem and implement something that doesn't help you.
- Avoid falling into the trap of the [XY Problem](https://xyproblem.info/).

## Pull Requests

We have a [pull request template](https://github.com/neomutt/neomutt/blob/main/.github/PULL_REQUEST_TEMPLATE.md) that guides you through the submission process. Please fill it out completely when opening a PR.

When submitting a pull request:

- The first line should be a short summary (50 characters or fewer) of your commit. If you can't find a concise one-line summary, consider splitting the commit into multiple commits.
- Keep one blank line between the summary and the body.
- Use bullet points in the commit message body to separate multiple changes.
- Keep your commits clear and concise. Avoid combining two features or bug fixes into one commit, as this makes them harder to understand. Don't hesitate to [rewrite](https://git-scm.com/book/en/v2/Git-Tools-Rewriting-History) the Git history of your development branch.
- Wrap the commit message body at approximately 80 characters.
- If your commit addresses a particular pull request (PR), commit, or issue, please reference it. See the GitHub documentation ([1](https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/autolinked-references-and-urls) and [2](https://docs.github.com/en/issues/tracking-your-work-with-issues/using-issues/linking-a-pull-request-to-an-issue)) for guidance.
- Eliminate any warnings that `gcc` or other tools produce during compilation.
- If your commit addresses only a specific method or file, mention it in the commit message.

Thank you for your contributions!

The NeoMutt Team
