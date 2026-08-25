# Documentation and comment review

**Review date:** 2026-08-24  
**Scope:** Root/docs Markdown, production C++ headers and implementations,
Doxygen configuration, repository documentation gates

## Outcome

The maintained documentation now has an explicit hierarchy:

1. [Project specification](../ProjectSpecification.md) defines product behavior,
   limits, dependency posture, and acceptance criteria.
2. [Architecture](ARCHITECTURE.md), [concurrency](CONCURRENCY_MODEL.md),
   [LAR1](LAR1_FORMAT.md), and the component/file references explain the design.
3. [Threat model](THREAT_MODEL.md) records assets, actors, trust boundaries,
   attack surfaces, exact parser/GPU budgets, residual risks, and review triggers.
4. [Developer guidance](DEVELOPER_GUIDE.md) and
   [quality gates](QUALITY_GATES.md) turn those contracts into change and release
   workflows.

The previous snapshot linked to a missing `ProjectSpecification.md`, described
the removed 10-million-record ceiling and per-record session index, documented
the superseded `finished -> deleteLater` shutdown, and repeated the obsolete Qt
6.5/6.8.3 posture. Those contradictions have been corrected.

## Comment quality remediation

Structural comment coverage was high, but much of the symbol-level prose merely
restated a method name (for example, “Performs the … operation” or “Returns the
current …”). This made generated API pages longer without explaining ownership,
failure behavior, units, bounds, threading, or transactional semantics.

The review removed 1,098 such Doxygen blocks across the production source tree.
It retained all production file summaries and semantic class/method contracts. Critical
interfaces now state the information a signature cannot convey:

- glTF canonical containment, symlink policy, and aggregate resource accounting;
- sparse 4,096-record session checkpoints, one-page cache, no arbitrary count
  cap, and reader thread confinement;
- synchronous worker drain/rehoming/join/destruction with no deferred-deletion
  dependency;
- the 0.65-pixel GPU eligibility proof and CPU fallback at sensitive zooms.

The policy is not “more comments.” It is fewer redundant comments and explicit
contracts at trust, resource, unit, lifetime, and state-transition boundaries.

## Enforced controls

| Gate | Enforced property |
| --- | --- |
| `check-doc-links` | Every repository-local Markdown target exists and stays inside the tree |
| `check-doc-coverage` | Every maintained production header has a file-level Doxygen summary |
| `check-doc-quality` | Known generated filler is absent and critical security/lifetime/resource language remains present |
| `check-docs` | Doxygen parses the complete production/documentation surface with warnings treated as failures |
| `check-repository` | Aggregates documentation gates with formatting and architecture checks |

The semantic gate intentionally checks source-adjacent contracts as well as
Markdown. A future refactor cannot remove the threat model, resource language,
thread-affinity ownership, sparse-index behavior, or pixel-error fallback while
leaving only structurally valid comments.

## Maintainer checklist

When changing behavior:

- update the project specification if the user-visible contract changes;
- update the threat model for a new input, codec, URI, queue, worker, plugin, or
  resource budget;
- document units, ownership/affinity, atomicity, failure state, and limits where
  they are non-obvious;
- remove comments that merely translate an identifier into a sentence;
- add a regression test and update the applicable quality-gate evidence;
- run `check-repository` before treating documentation as complete.

## Local verification record

The final audit run records exact command results in
[Project audit report](PROJECT_AUDIT_REPORT.md). The documentation-specific
portable gates are also directly runnable:

```bash
python3 tools/check_doc_links.py .
python3 tools/check_doxygen_coverage.py .
python3 tools/check_doc_quality.py .
```
