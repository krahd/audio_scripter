# Cross-Repository Administration

Global repository registry, cross-domain status, and the master calendar are maintained in `krahd/tom-work-admin`.

This public repository remains canonical for **audio_scripter** implementation, releases, examples, tests, exact implemented language documentation, and reviewed public technical/research state.

The private research and first-paper workspace is:

`krahd/academic-writing/my_papers_2026/2026 - Programmable Audio Language/`

It is canonical for unpublished theory, detailed prior-art/novelty analysis, benchmark methodology, manuscript development, private reflective/artistic-research material, funding strategy, and graduate-research planning.

Submission-specific packages belong in `krahd/professional-opportunities`; grant/funding application packages belong in `krahd/grant-applications`.

## Mandatory synchronisation rule

`krahd/tom-work-admin` **must be kept current** whenever work here or in the private research workspace materially changes the project's administratively meaningful state. Updating the administration repository is part of completing the change, not optional later cleanup.

Update `krahd/tom-work-admin` in the same work session when any of the following changes:

- project lifecycle state, scope, artistic/research direction, or major implementation goal;
- release/version, plugin/platform compatibility, distribution, test status, or major validation milestone;
- relationship to a manuscript, submission, grant, collaborator, repository, host/DAW, or other cross-domain dependency;
- deadline, release target, presentation, submission/publication outcome, or other material cross-domain date;
- working/public project name or naming status;
- current next action or major artistic/research/technical gate.

## Ownership boundary

### Keep here — public implementation repository

- source code;
- examples;
- releases/build configuration;
- tests and reproducible technical evidence;
- exact current language specification;
- verified, conservative public documentation;
- implementation limitations and engineering status.

### Keep in private `academic-writing`

- novelty claims under investigation;
- detailed system comparisons/evaluative judgements;
- theoretical development;
- paper arguments and drafts;
- raw/sensitive reflective-practice logs;
- unpublished artwork concepts when disclosure is undesirable;
- venue/funding/PhD strategy.

Do not copy private material into this public repository merely for administrative completeness. `tom-work-admin` should store the concise cross-repository relationship and point to the canonical sources.

## Completion check

Before considering a material project-state change complete, verify that:

1. this repository reflects implementation/public-state changes;
2. the private academic-writing workspace reflects unpublished research/paper changes;
3. `krahd/tom-work-admin` reflects resulting global status, relationships, naming state, gates, dates, and next actions;
4. related submission/grant repositories are updated only when their domains are actually affected;
5. no stale cross-domain status or date remains in `tom-work-admin`.