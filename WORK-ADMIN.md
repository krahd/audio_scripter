# Cross-Repository Administration

Global repository registry, cross-domain status, and the master calendar are maintained in `krahd/tom-work-admin`.

This public repository remains canonical for **audio_scripter** implementation, releases, examples, tests, exact implemented language documentation, and reviewed public technical/research state.

The canonical private research workspace is:

`krahd/research/projects/audio_scripter/`

It owns unpublished theory, language-ontology/design work, detailed prior-art/novelty analysis, benchmark methodology, private reflective/artistic-research material, first-paper development before a distinct manuscript workspace is warranted, funding strategy, and graduate-research planning.

When a paper becomes a distinct publication object, its manuscript belongs under `krahd/research/academic-writing/my_papers_<year>/` and should link to the project dossier rather than duplicate it.

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

### Keep in private `krahd/research/projects/audio_scripter/`

- novelty claims under investigation;
- detailed system comparisons/evaluative judgements;
- theoretical and ontological development;
- language-design alternatives and failed semantic experiments;
- raw/sensitive reflective-practice logs;
- unpublished artwork concepts when disclosure is undesirable;
- paper/funding/PhD strategy before those become separate canonical domain objects.

Do not copy private material into this public repository merely for administrative completeness. `tom-work-admin` stores the concise cross-repository relationship and points to canonical sources.

## Completion check

Before considering a material project-state change complete, verify that:

1. this repository reflects implementation/public-state changes;
2. `krahd/research/projects/audio_scripter/` reflects unpublished research/language-design changes;
3. `krahd/tom-work-admin` reflects resulting global status, relationships, naming state, gates, dates, and next actions;
4. related manuscript/submission/grant repositories are updated only when their domains are actually affected;
5. no stale cross-domain status or date remains in `tom-work-admin`.