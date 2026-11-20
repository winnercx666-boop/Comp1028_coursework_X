# AI-Assistance Record Draft

Use this factual draft when completing the course's `COMP1028_AI_Usage_Log_Template.docx`. It is not a signed student declaration and does not claim that a student personally performed every implementation, test or presentation activity.

**Date:** 20 November 2026. **Tool:** Codex.

## Requested task and source material

The user requested implementation of Feature 6 (Statistics and Reporting) and Feature 9 (Persistent Index) against the existing group repository, followed by upload to that repository and an English-language submission version.

The work was checked against the Moodle-downloaded coursework v1 PDF and `WinnerCX666-BOOP/Comp1028_coursework_X` at original revision `2012ef8`.

## AI-assisted work

- Interpret the feature requirements and distinguish total occurrences, unique terms and term-document relationships.
- Extract the original function-local index into a shared interface.
- Implement TXT/CSV statistics reports and versioned index save/load.
- Integrate the console menu while retaining ASCII tokenization, lowercase normalization, duplicate-query-term handling and frequency-sum ranking.
- Introduce dynamically allocated document content and standard-C collection-list input, with the original folder interface retained as an optional adapter.
- Write function/file-format documentation, a code-reading guide and independent contract/process tests.
- Fix startup/build-path consistency before publishing, upload the reviewed source through the authorized GitHub account, and prepare the pull request.
- Convert submission documentation and pull-request text into English at the user's request.

## Actual verification

- Both application variants compiled under strict C17 with zero errors or warnings.
- Module contract tests reported 1012 checks and zero failures, including 21 malformed-index variants.
- All 28 menu/new-process checks passed, including a synthetic 50-document/100000-occurrence scale check.
- Uploaded source was fetched back from GitHub and compared with local files; fresh-source compilation and sample restart/loading behavior passed.
- Detailed scope and reproduction commands are in `docs/validation.md`.

## Student follow-up to record truthfully

Students should record the work they actually perform: reading and explaining contributed functions, tracing sample counts, compiling on their own machines, demonstrating save/exit/load behavior, integrating with the group, correcting any AI suggestions, and measuring the prescribed benchmark data.

Any subsequent modification and its verification should be recorded as an actual activity. A prepared demonstration outline is not evidence that a presentation has already been recorded or submitted.

The running application does not call LLMs, AI APIs, semantic-search services or ready-made retrieval frameworks. AI assistance was used during development and documentation, with the scope disclosed here.
