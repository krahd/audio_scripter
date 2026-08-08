# Cross-Repository Administration

Global repository registry, cross-domain status, and the master calendar are maintained in `krahd/tom-work-admin`.

This repository remains canonical for **audio_scripter** implementation, releases, examples, tests, and project-specific technical/artistic state.

Any paper manuscript or publication artefact belongs canonically in `krahd/academic-writing`; submission-specific packages belong in `krahd/professional-opportunities`; grant/funding packages belong in `krahd/grant-applications`.

## Mandatory synchronisation rule

`krahd/tom-work-admin` **must be kept current** whenever work here materially changes the project's administratively meaningful state. Updating the administration repository is part of completing the change, not optional later cleanup.

Update this repository first for substantive project changes, then update `krahd/tom-work-admin` in the same work session when any of the following changes:

- project lifecycle state, scope, artistic/research direction, or major implementation goal;
- release/version, plugin/platform compatibility, distribution, test status, or major validation milestone;
- relationship to a manuscript, submission, grant, collaborator, repository, host/DAW, or other cross-domain dependency;
- deadline, release target, presentation, submission/publication outcome, or other material cross-domain date;
- current next action or major artistic/research/technical gate.

## Ownership boundary

Keep source, examples, releases, tests, and project-specific artistic/technical evidence here. `tom-work-admin` stores only the concise cross-repository view and must point back to canonical project sources rather than duplicate them.

## Completion check

Before considering a material project-state change complete, verify that:

1. this repository reflects the substantive change;
2. `krahd/tom-work-admin` reflects any resulting global status, date, relationship, or next-action change;
3. related domain repositories are updated when the change affects manuscripts, submissions, or grants;
4. no stale cross-domain status or date remains in `tom-work-admin`.
