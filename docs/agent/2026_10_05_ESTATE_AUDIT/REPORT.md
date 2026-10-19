# Estate audit: sushidsp

**Status:** Shipped — 2026-10-05. Read-only audit; nothing in the repository was changed.

One auditing agent per dimension read the tree and a second agent checked each finding against
the files. A finding marked *confirmed* was re-read by the reviewer; *unclear* means the reviewer
could not settle it; *found by review* means the first pass missed it and nobody re-checked it.
Refuted findings are listed and not counted. No build, test or project CLI was run.

| Dimension | High | Medium | Low | Refuted |
| --- | --- | --- | --- | --- |
| Licence | 1 | 10 | 5 | 0 |
| Command line | 2 | 14 | 9 | 0 |
| Documentation | 4 | 14 | 3 | 0 |
| Layout and hygiene | 3 | 15 | 6 | 1 |
| Code shape | 5 | 13 | 4 | 0 |

## Licence

### Facts

- D:/Projects/sushidsp/LICENSE is the unmodified Apache License 2.0 text, 201 lines; its appendix at line 189 still reads 'Copyright [yyyy] [name of copyright owner]' (the template placeholder, which is normal for the licence text itself).
- D:/Projects/sushidsp/NOTICE: 'SushiDSP / Copyright (c) 2026-present Mustafa Garip & Sushi Systems', states Apache-2.0, carries a trademark section (Mesa/Boogie, Marshall, Peavey, Diezel, Ibanez, Boss, Yamaha) and a third-party table (SDL 2.30.0 zlib, nlohmann/json 3.11.3 MIT, GLFW 3.4 zlib, Dear ImGui 1.90.9 MIT, GoogleTest 1.14.0 BSD-3).
- Only other licence document tracked: D:/Projects/sushidsp/third_party/dr_libs/LICENSE.txt (Unlicense or MIT-0, 'Copyright 2023 David Reid', plus the minimp3 CC0 notice). No docs/LICENSE, no COPYING, no per-package LICENSE under cli/ or modules/.
- Tracked files: 681 (264 .cpp, 189 .hpp, 1 .h, 28 .py, 3 .cmake, 19 CMakeLists.txt, 116 .md, rest data and config).
- C++ header variant A (boxed 76-column Apache block, 'Copyright (c) 2026-present Mustafa Garip & Sushi Systems'): 445 of 453 .cpp/.hpp. Examples: apps/host/SineToneNode.hpp, apps/host/main.cpp, apps/host_gui/AudioImportCache.hpp.
- C++ header variant B (same text, last notice line one space short, 75 columns): 4 files - include/SushiDSP/core/DenormalGuard.hpp, tests/benchmark/Benchmark.hpp, tests/benchmark/main.cpp, tests/functional/unit/test_denormal_guard.cpp.
- C++ header variant C (Apache block missing the line 'You may obtain a copy of the License at'): 3 files - modules/nodes/cabinet/include/SushiDSP/cabinet/CabinetNode.hpp, modules/nodes/cabinet/src/CabinetNode.cpp, tests/functional/unit/test_cabinet_node.cpp.
- C++ files with no licence header: 1 - tests/functional/unit/test_late_reverb.cpp (opens directly with the @file block). The one .h, third_party/dr_libs/dr_mp3.h, is third-party and carries its own licence at its foot.
- Python header variant (two lines: '# Copyright (c) 2026-present Mustafa Garip & Sushi Systems' / '# Licensed under the Apache License, Version 2.0. See LICENSE.'): 28 of 28 .py; tools/spice/export.py has it after a shebang. Examples: cli/sushidsp/__init__.py, cli/sushidsp/__main__.py, cli/sushidsp/cli.py. Python files with none: 0.
- CMake: 0 of 22 files carry a licence header (19 CMakeLists.txt, 3 under cmake/). Examples: CMakeLists.txt, cmake/Sanitizers.cmake, tests/CMakeLists.txt. No GLSL or TypeScript in the tree.
- Machine-readable declarations: cli/pyproject.toml (name sushidsp-cli, version 0.1.0) has no license field and no classifiers; sushi-module.toml has no licence key; CMakeLists.txt line 2 'project(SushiDSP VERSION 0.1.0 LANGUAGES CXX)' declares none; Doxyfile has none (PROJECT_NUMBER 'v1 (unreleased)'). No package.json, Dockerfile, conda or vcpkg manifest. No SPDX identifiers anywhere.
- Prose statements: README.md:86-90 ('Apache License, Version 2.0. See LICENSE' plus NOTICE pointer); docs/CONTRIBUTING.md:8-9 (contributions are licensed under Apache-2.0) and :67 (header template rule); docs/design/SUSHIDSP.md:20 and :37-49 (pins the Apache header template); assets/hrtf/README.md:14-15, :88-89; assets/ir/README.md:10-11; docs/reference/CHANGELOG.md:56.
- 36 files under docs/archive and docs/agent also mention Apache or the licence (frozen or agent-written records, for example docs/agent/2026_09_20_V0_1_0_RELEASE/PLAN.md:267).
- Copyright holder everywhere: 'Mustafa Garip & Sushi Systems', year form '2026-present'. No other holder or year in first-party source. @author tags: 177 'Mustafa Garip & Sushi Systems', 16 'Mustafa Garip'.
- git shortlog -sne --all: 654 Mustafa Garip <mustafa.garip3434@hotmail.com>, 1 Mustafa Garip <51916290+sushimg@users.noreply.github.com>. 655 commits, 2026-07-24 to 2026-10-04. One person, two addresses.
- Published state: no local tags, git ls-remote --tags origin returns nothing, gh reports the GitHub repo SushiSystems/SushiDSP as private, 0 forks, no releases, licence detected Apache-2.0. PyPI returns 404 for sushidsp-cli. The changelog has a 'v0.1.0 - 2026-10-03' heading with no tag behind it.
- Fetched by CMake FetchContent: SDL release-2.30.0 (zlib, CMakeLists.txt:75-78), nlohmann/json v3.11.3 (MIT, :139-142), GLFW 3.4 (zlib, :153-156), Dear ImGui v1.90.9 (MIT, :163-166), GoogleTest v1.14.0 (BSD-3, tests/CMakeLists.txt:22-25). Notices kept in NOTICE only.
- Vendored: third_party/dr_libs/dr_mp3.h, tag mp3-0.7.3, Unlicense or MIT-0, derived from minimp3 (CC0); README.md and LICENSE.txt beside it. Used only by apps/host_gui/Mp3Decoder.cpp.
- Steinberg ASIO SDK: not tracked (third_party/asio/ is ignored at .gitignore:20) but present on this disk; CMakeLists.txt:100-132 compiles it into sushi_dsp_asio when found on Windows and calls it 'proprietary (accept-the-license, dual Steinberg/GPLv3)'.
- Python dependencies in cli/pyproject.toml: sushicore>=0.7.0 (PyPI shows sushicore 0.4.0 published with license_expression Apache-2.0), typer>=0.12 (MIT), rich>=13.0 (MIT), tomli>=2.0 (MIT). Tools import numpy, scipy (BSD-3) and pytest (MIT); no requirements file declares them. Licences of typer, rich, tomli, numpy, scipy, pytest are from general knowledge, not read here.
- cli/sushistack.deps.toml declares intel-llvm (optional compiler, Apache-2.0 with LLVM exception, not linked) and sdl2 (apt libsdl2-dev / vcpkg sdl2, zlib).
- Third-party data in the tree: assets/hrtf/sadie2_ku100_48k.hrtf, converted from the SADIE II database, 'Copyright 2018, University of York', Apache-2.0, notice and citation in assets/hrtf/README.md:95-117.
- Derived measurement fixtures: tests/fixtures/bras_cr2_reference.csv computed from BRAS (CC-BY-SA-4.0, tests/fixtures/bras_cr2_reference.README.md:5-7); tests/fixtures/motus_reference.csv computed from Motus (CC-BY-4.0, tests/fixtures/motus_reference.README.md:3-5).
- docs/reference/spice/models/*.sub (12ax7_koren, el34_koren, d1n914) are written in-house from published Koren/Norman parameters and carry no licence line; the .asc schematics and CSVs beside them are generated by tools/spice/export.py.

### Corrections from review

- CMake file count: the audit says 19 CMakeLists.txt and 22 CMake files. git ls-files gives 18 CMakeLists.txt plus 3 under cmake/, so 21. The nineteenth .txt is third_party/dr_libs/LICENSE.txt, not a CMakeLists.txt. F7's title and '17 more' inherit the error (should be 21 and 16). The conclusion (none carries a header) holds.
- First-party header count in F9: '481 first-party headers' should be 480 (452 C++ with a header + 28 Python); tests/functional/unit/test_late_reverb.cpp has none. F8's '453 C++ ... headers' should be 452 for the same reason.
- Facts say the SPICE .asc schematics are 'generated by tools/spice/export.py' and the models are in-house; docs/reference/spice/ts_tone/ts_tone.asc:4 states in its own text that it was 'Traced from the ElectroSmash TS808 tone/volume schematic and R.G. Keen', so at least that schematic is a derived drawing, not purely generated.
- Recomputed and correct: 681 tracked files; 264 .cpp, 189 .hpp, 1 .h, 28 .py, 116 .md; C++ variants 445 / 4 / 3 / 1; Python 28 of 28; @author 177 / 16; shortlog 654 + 1; 36 archive/agent files mentioning the licence.

### Findings

#### L1. [high] Steinberg ASIO SDK is compiled into the host under its own proprietary/GPLv3 terms

Evidence: D:/Projects/sushidsp/CMakeLists.txt:100-132 builds ${ASIO_SDK_DIR}/common/asio.cpp, host/asiodrivers.cpp and host/pc/asiolist.cpp into sushi_dsp_asio; line 100 says 'proprietary (accept-the-license, dual Steinberg/GPLv3)'. apps/host_gui/CMakeLists.txt:61 sets SUSHIDSP_HAVE_ASIO=1. third_party/asio/ exists on disk, ignored at .gitignore:20. NOTICE does not mention ASIO. The dependencies skill rejects GPL and vendor SDK licences that require marks or restrict redistribution.

Recommendation: Decide before any binary is distributed: a build with ASIO falls under either the Steinberg proprietary agreement (signed licence, logo and trademark duties) or GPLv3. GPLv3 cannot be combined with a non-commercial outbound licence, so only the Steinberg agreement route works for a shipped binary. Record the choice in NOTICE and a design document; the source tree itself is unaffected because the SDK is not tracked.

Review: confirmed. CMakeLists.txt:100-132 compiles three SDK files into sushi_dsp_asio; third_party/asio/LICENSE.txt on disk states the dual Steinberg ASIO License / GPLv3 terms and the signed-agreement duty; .gitignore:20 ignores the folder; NOTICE has no ASIO line. The audit omits that the same block also adds apps/host_asio (CMakeLists.txt:130), a second binary that links the SDK.

#### L2. [medium] A fixture derived from a CC-BY-SA-4.0 dataset would sit inside a non-commercial repository

Evidence: D:/Projects/sushidsp/tests/fixtures/bras_cr2_reference.csv, described in tests/fixtures/bras_cr2_reference.README.md:5-7 as computed from BRAS, 'licence CC-BY-SA-4.0'. NOTICE does not list BRAS or Motus. If the CSV counts as adapted material, ShareAlike requires it to stay under CC-BY-SA-4.0, which permits commercial use and so cannot be relicensed under non-commercial terms.

Recommendation: State in the new licence scope (and in NOTICE) that tests/fixtures/bras_cr2_reference.csv stays under CC-BY-SA-4.0 and tests/fixtures/motus_reference.csv carries CC-BY-4.0 attribution, or drop the CSV and regenerate it locally with tools/dsp/bras_reference.py. Whether eight rows of computed T30 values are copyrightable at all is a question for a lawyer, not settled here.

Review: confirmed. tests/fixtures/bras_cr2_reference.README.md:5-7 names BRAS and CC-BY-SA-4.0; motus_reference.README.md:3-4 names CC-BY-4.0; NOTICE lists neither. The copyrightability caveat is stated once and in the right place.

#### L3. [medium] Third-party Apache-2.0 data ships in the tree and its notice is missing from NOTICE

Evidence: D:/Projects/sushidsp/assets/hrtf/sadie2_ku100_48k.hrtf is a converted copy of the SADIE II set, 'Copyright 2018, University of York', Apache-2.0 (assets/hrtf/README.md:105-109). NOTICE's third-party table lists only SDL, nlohmann/json, GLFW, Dear ImGui and GoogleTest. assets/hrtf/README.md:88-89 justifies shipping without a second licence file because the repository is itself Apache-2.0; that argument ends with the relicence.

Recommendation: Keep the file under Apache-2.0 after the change: add SADIE II (holder, licence, statement that the file was modified by convert_hrtf.py) to NOTICE, keep a copy of the Apache-2.0 text for it (for example assets/hrtf/LICENSE), and rewrite assets/hrtf/README.md:14-15 and :88-89, which name Apache-2.0 as the project licence.

Review: confirmed. assets/hrtf/README.md:108 carries 'Copyright 2018, University of York' Apache-2.0 text, :88-89 argue from the repository's own Apache licence; NOTICE's table has five rows and no SADIE entry.

#### L4. [medium] dr_mp3 is vendored but absent from NOTICE

Evidence: D:/Projects/sushidsp/third_party/dr_libs/dr_mp3.h (mp3-0.7.3, Unlicense or MIT-0, with minimp3 CC0) is compiled into apps/host_gui through Mp3Decoder.cpp. D:/Projects/sushidsp/NOTICE lists five fetched libraries and not this one. third_party/dr_libs/README.md and LICENSE.txt exist, so the folder itself is in order.

Recommendation: Add a dr_libs row to the NOTICE third-party table pointing at third_party/dr_libs/LICENSE.txt. No conflict with a non-commercial outbound licence; the file keeps its own terms.

Review: confirmed. third_party/dr_libs/{dr_mp3.h,LICENSE.txt,README.md} are tracked, README names tag mp3-0.7.3 and Mp3Decoder.cpp; NOTICE does not list dr_libs.

#### L5. [medium] No machine-readable licence declaration exists, so the declarations cannot agree or disagree

Evidence: D:/Projects/sushidsp/cli/pyproject.toml has no 'license' key and no classifiers; D:/Projects/sushidsp/sushi-module.toml has no licence key; no SPDX-License-Identifier in any file. GitHub detects Apache-2.0 from LICENSE alone.

Recommendation: When the new licence is chosen, declare it once in cli/pyproject.toml (license expression plus license-files) and, if the module manifest format gains a licence key, in sushi-module.toml. A custom source-available licence has no SPDX id unless an existing one such as PolyForm-Noncommercial-1.0.0 is picked; use 'LicenseRef-...' otherwise.

Review: confirmed. cli/pyproject.toml has no license key or classifiers, sushi-module.toml has four keys and none is a licence, git grep -il SPDX returns nothing. gh api reports license Apache-2.0.

#### L6. [medium] Eight C++ files have a missing or malformed licence header

Evidence: None: D:/Projects/sushidsp/tests/functional/unit/test_late_reverb.cpp (line 1 is '/**' of the @file block). Missing the 'You may obtain a copy of the License at' line: modules/nodes/cabinet/include/SushiDSP/cabinet/CabinetNode.hpp, modules/nodes/cabinet/src/CabinetNode.cpp, tests/functional/unit/test_cabinet_node.cpp. Last notice line 75 columns instead of 76: include/SushiDSP/core/DenormalGuard.hpp:21, tests/benchmark/Benchmark.hpp, tests/benchmark/main.cpp, tests/functional/unit/test_denormal_guard.cpp.

Recommendation: The header rewrite must not assume one exact old block: a literal search-and-replace of variant A leaves these eight untouched. Replace the block by its position (line 1 to the closing box line) and add the header to test_late_reverb.cpp.

Review: confirmed. Recomputed: test_late_reverb.cpp opens with the @file block; the three cabinet files lack 'You may obtain a copy'; the four named files have a 75-column last notice line (448 files at 76, 4 at 75). The recommendation (replace by position, not by literal match) is a sound single-rule fix.

#### L7. [medium] Prose and rule documents that state Apache-2.0 and must change with the licence

Evidence: D:/Projects/sushidsp/README.md:86-90; docs/CONTRIBUTING.md:8-9 (inbound contributions 'licensed under the project's Apache License, Version 2.0') and :67; docs/design/SUSHIDSP.md:20 and :37-49 (the pinned header template); assets/hrtf/README.md:14-15, :88-89; assets/ir/README.md:10-11 ('compatible with this project's Apache License 2.0'); NOTICE lines 4-6. Plus 453 C++ and 28 Python headers.

Recommendation: Rewrite these in the same commit as LICENSE. The asset READMEs need a new admission rule, since 'compatible with Apache-2.0' no longer describes what may be bundled. docs/CONTRIBUTING.md needs an inbound term that gives the owner the right to license contributions under the new licence (a CLA or an explicit grant), because 'inbound = outbound' under a non-commercial licence would leave the owner without commercial rights in others' contributions. docs/archive and docs/agent (36 files) are frozen records and stay as written.

Review: confirmed. README.md:86-90, docs/CONTRIBUTING.md:8-9 and :67, docs/design/SUSHIDSP.md:20 and :39-49, assets/hrtf/README.md:14 and :88-89, assets/ir/README.md:10-12, NOTICE:4-6 all read as cited; 36 archive/agent files recounted. '453 C++ headers' should be 452. The audit leaves out docs/reference/CHANGELOG.md:56 and tools/dsp/convert_hrtf.py:901, which keys on the string 'Apache License'.

#### L8. [medium] A tracked schematic says it was traced from third-party drawings whose licence is not recorded

Evidence: D:/Projects/sushidsp/docs/reference/spice/ts_tone/ts_tone.asc:4 reads 'Traced from the ElectroSmash TS808 tone/volume schematic and R.G. Keen, "The Technology of the Tube Screamer" (geofex.com)'; docs/reference/spice/ts_tone/README.md:76 and :122 read component values 'off the ElectroSmash drawing'. modules/nodes/mesa_mark2c/README.md:12 and include/SushiDSP/circuits/PowerAmpFeedback.hpp:62 rest on 'jrb32's traced schematic' and a factory sheet. The audit listed this under not_checked and at the same time recorded the .asc files as generated in-house. NOTICE names none of these sources.

Recommendation: Record the licence of each source drawing (ElectroSmash, geofex, jrb32's trace, the Mesa factory sheet) in docs/reference/spice/README.md or NOTICE, and confirm that only component values and topology were taken, not a copy of the drawing. If ElectroSmash's terms are non-commercial or share-alike, ts_tone.asc carries that term whatever the new outbound licence says. Check docs/archive for stored copies of the source scans; that was not done here.

Review: found by review.

#### L9. [medium] No checker verifies the licence block, so the header rewrite has nothing to hold it in place

Evidence: D:/Projects/sushidsp/tools/documentation/check_source_comments.py:29 and :54-73 only locate the leading boxed block (any run of '/* ... */' lines) in order to skip it; no line compares its text, its width or its presence. That is how tests/functional/unit/test_late_reverb.cpp (no block), the three cabinet files (one line missing) and four 75-column files reached main. The source-comments skill says this checker 'enforces every rule above', and the licence block is the first rule.

Recommendation: Add one rule to check_source_comments.py that compares each file's opening block with a single template held in one place (file name on line 2, fixed notice text, 76 columns; the two-line form for Python), and land it in the same change as the new header. The rewrite then has an acceptance test and the eight drifted files cannot recur.

Review: found by review.

#### L10. [medium] The pinned header template lives in the source-comments skill and in the design document, not only in the files

Evidence: C:/Users/sushi/.claude/skills/source-comments/SKILL.md shows the block as '/* ... Apache-2.0 notice ... */'; D:/Projects/sushidsp/docs/design/SUSHIDSP.md:37-49 pins the full Apache text and docs/CONTRIBUTING.md:67 points at it. F10 asks for the dependencies skill to change but not this one, so an agent writing a new file after the relicence would still stamp the Apache block.

Recommendation: Change the template in the source-comments skill and in docs/design/SUSHIDSP.md before the per-file rewrite starts, so the files, the skill and the checker rule above all read from one text.

Review: found by review.

#### L11. [medium] ASIO has a second binary and a trademark duty the audit does not list

Evidence: D:/Projects/sushidsp/CMakeLists.txt:130 adds apps/host_asio whenever the SDK is found, beside apps/host_gui; third_party/asio/ on disk holds 'Steinberg ASIO Usage Guidelines.pdf' and 'Steinberg ASIO Logo Artwork', and LICENSE.txt there requires a signed agreement 'before publishing a software under the proprietary license'. NOTICE's trademark section (lines 8-24) names seven amplifier makers and not Steinberg. SUSHIDSP_BUILD_ASIO defaults ON (CMakeLists.txt:106), so any Windows developer build with the folder present links the SDK.

Recommendation: Fold into F1's decision: name both targets, add the ASIO mark to NOTICE's trademark section if the Steinberg route is taken, and consider defaulting SUSHIDSP_BUILD_ASIO to OFF for release builds until the signed agreement exists. The last point is a build-configuration change and needs the owner's go-ahead.

Review: found by review.

#### L12. [low] 22 CMake files carry no licence header

Evidence: D:/Projects/sushidsp/CMakeLists.txt, cmake/CompilerWarnings.cmake, cmake/Sanitizers.cmake, cmake/SushiDSPCompilerFlags.cmake, tests/CMakeLists.txt and 17 more CMakeLists.txt under apps/ and modules/. The source-comments skill names C++, GLSL, TypeScript and Python, not CMake, so this is a gap in coverage and not a rule breach.

Recommendation: Decide once for all repositories whether build files carry the header. If yes, it is a build-configuration edit and needs the owner's go-ahead under the standing build ban for agents.

Review: confirmed. No CMake file carries a copyright line, but the count is 21 (18 CMakeLists.txt + 3 cmake/), not 22; the '17 more' in the evidence should read 16. Severity low is right, since the source-comments skill does not name CMake.

#### L13. [low] Copyright holder and author strings name a person and an organisation without saying which one owns the code

Evidence: All 481 first-party headers and D:/Projects/sushidsp/NOTICE:2 read 'Copyright (c) 2026-present Mustafa Garip & Sushi Systems'. @author is 'Mustafa Garip & Sushi Systems' in 177 blocks and 'Mustafa Garip' in 16; the source-comments skill says @author names a person.

Recommendation: Fix the holder string before the rewrite touches every file: if Sushi Systems is a registered entity, an assignment or a clear statement of who licenses the code is needed; if it is a trade name of the owner, one holder is cleaner. Normalise @author to the person in the same pass.

Review: confirmed. @author recount: 177 'Mustafa Garip & Sushi Systems', 16 'Mustafa Garip'; the skill says @author names a person. The header total is 480 (452 C++ + 28 Python), not 481.

#### L14. [low] The dependencies skill and the asset READMEs reject the licence class the owner is moving to

Evidence: C:/Users/sushi/.claude/skills/dependencies/SKILL.md licence table: 'Non-commercial, source-available, field-of-use limits: Rejected'. D:/Projects/sushidsp/cli/pyproject.toml:11 depends on sushicore>=0.7.0, a sibling that will itself become non-commercial. assets/ir/README.md:11 refuses 'anything with a non-redistributable or non-commercial clause'.

Recommendation: Amend the skill so first-party Sushi Systems packages under the new licence are accepted, and keep the rejection for third-party code. Not a defect in this repository; it will block agents working here after the change.

Review: confirmed. dependencies skill table row 'Non-commercial, source-available, field-of-use limits: Rejected' read from the loaded skill; assets/ir/README.md:11-12 and cli/pyproject.toml:11 read as cited. Correctly marked as not a defect of this repository.

#### L15. [low] Nothing in this repository is irrevocably published under Apache-2.0, as far as can be seen today

Evidence: git tag -l and git ls-remote --tags origin: empty. gh api repos/SushiSystems/SushiDSP: private true, forks 0, releases none. PyPI https://pypi.org/pypi/sushidsp-cli/json: 404. D:/Projects/sushidsp/docs/reference/CHANGELOG.md:7 has a 'v0.1.0 - 2026-10-03' heading with no tag. One author in git shortlog, so no outside contributor has to consent.

Recommendation: Relicense before the repository is made public or tagged. The one exposure is indirect: sushicore 0.4.0 is on PyPI under Apache-2.0 and stays so for that version. Whether this repository was ever public, or shared as a copy with anyone, cannot be read from the tree and the owner should confirm it.

Review: confirmed. Re-ran: git tag -l empty, gh api gives private true / forks 0 / 0 releases / Apache-2.0, PyPI sushidsp-cli 404, sushicore 0.4.0 Apache-2.0; CHANGELOG.md:7 is '## v0.1.0 — 2026-10-03'; shortlog shows one person with two addresses.

#### L16. [low] Trademark section and in-house SPICE models have no stated licence scope

Evidence: D:/Projects/sushidsp/NOTICE lines 8-24 handle third-party amplifier marks only. docs/reference/spice/models/12ax7_koren.sub, el34_koren.sub and d1n914.sub, eight .asc schematics, the .csv references and tests/golden/*.f32 carry no licence line and are not source files with headers.

Recommendation: Say in LICENSE scope or NOTICE that data, schematics, golden files and documentation fall under the same licence (or a named different one, such as a CC non-commercial licence for docs), and add a clause reserving the Sushi Systems and SushiDSP names, which Apache-2.0 section 6 covered until now.

Review: confirmed. NOTICE:8-24 covers third-party marks only; 3 .sub, 8 .asc, 20 .csv and 12 .f32 files are tracked with no licence line. Low is right. The claim that the .sub and .asc files are all in-house is too strong for ts_tone.asc, see missed.

### Not checked

- The 116 Markdown files were searched by keyword (apache, licence, copyright, SPDX, non-commercial), not read in full; a licence statement phrased without those words would be missed.
- The 36 files under docs/archive and docs/agent that mention the licence were counted, not read.
- Licences of typer, rich, tomli, numpy, scipy, pytest and of the fetched SDL, nlohmann/json, GLFW, Dear ImGui and GoogleTest were not read from their sources; NOTICE's table and general knowledge were taken as given. Their transitive dependencies were not traced.
- The contents of the untracked third_party/asio/ folder and its actual licence file were not opened; the characterisation is the one in CMakeLists.txt:100.
- The actual installed version of sushicore (pyproject asks >=0.7.0, PyPI shows 0.4.0) and its licence metadata beyond the PyPI JSON were not examined.
- Whether the GitHub repository was public at any earlier time, and whether copies were given to third parties, cannot be determined from the tree.
- Provenance of the JCM800 and Tube Screamer .asc schematics (drawn in-house or traced from a third-party schematic) was not verified; docs/reference/CHANGELOG.md:75 mentions a traced Mark IIC+ schematic by 'jrb32' that was not followed up.
- build/, .claude/, .superpowers/ and imgui.ini are untracked and were skipped.
- No legal judgement was made on whether the new licence can be applied; findings F2 and F9 name questions for a lawyer.
- Binary contents of .hrtf and .f32 files were not inspected for embedded notices.

## Command line

### Facts

- Console scripts: `sd` and `sushidsp`, both -> `sushidsp.cli:app` (D:/Projects/sushidsp/cli/pyproject.toml:18-20); `python -m sushidsp` via cli/sushidsp/__main__.py. Distribution name `sushidsp-cli` 0.1.0, requires-python >=3.10.
- Framework: Typer (>=0.12) with rich (>=13); root app built with cls=sushicore.typer_help.help_group(console.current), no_args_is_help=True, add_completion=False, rich_markup_mode='rich' (cli/sushidsp/cli.py:27-36).
- sushicore requirement: `sushicore>=0.7.0` (cli/pyproject.toml:12); the sushicore checkout at D:/Projects/sushicore is at version 0.7.0. All six sibling CLIs pin the same floor.
- Command tree (12 commands, no sub-apps): configure, build, test, doxygen, check [panel Build]; run [panel Application]; config, env, setup, doctor, link, unlink [panel Environment].
- configure: --type/-t debug|release|relwithdebinfo, --define/-D VAR=VALUE (repeatable). build: --type/-t, --clean, --define/-D. test: --type/-t, --suite/-s unit|regression|benchmark|all (default unit), --clean.
- doxygen: no options. check: --report/-r, --rule <name>. run: --type/-t, [EXTRA_ARGS]... config: no options. env: --all/-a.
- From sushicore (register_provision_commands, cli.py:170-176): setup (--dry-run, --yes, --toolchain repeatable, --no-gpu), doctor (--for GROUP), link (--workspace), unlink (--workspace).
- Taken from sushicore: help page (typer_help.help_group), LazyConsole, ModuleProfile/ModuleConfig (root finding, layered config), ProvisionSettings, Diagnostics (config/env commands), Runner, CMakeDriver (configure/compile/doxygen), cmake_cache, build_env (vcvars snapshot/cache), toolchain_args.c_compiler_for.
- Local in sushidsp: BuildType/Suite enums and build-type resolution, compiler search (Config.resolved_compiler/compiler_source/resolved_generator), _remove_tree, the ctest invocation, the `check` runner, the `run` launcher, _DOXYGEN_HINT, env cache key and toolchain PATH injection (cli/sushidsp/env.py).
- Help panels: three, named Build / Application / Environment (cli.py:23-25). Every command carries an `epilog` of example lines; tests/test_help_screen.py asserts the three groups and the examples.
- Output: Rich through sushicore's Console (themed, [cli] config table). No JSON output, no --describe, no --quiet/--verbose on any command. Child processes (cmake, ctest, the host GUI, the checkers) inherit stdout directly.
- Version flag: none. `sd --version` is not defined; se has `--version`, hub has `--describe`.
- Exit codes: each command does `raise typer.Exit(<service return code>)`, so the child's code is passed through; Click usage errors give 2; setup/doctor/link use 2 for usage-class failures and 1 otherwise; a missing executable gives 1 (Runner default); `sd run` returns 1 when the binary is not found and 0 on Ctrl+C.
- Errors: outside a checkout ModuleConfig.find_project_root raises SystemExit(message) (one line on stderr, exit 1). Service failures print through console.error/console.warn. No module base exception class and no top-level handler exist in cli/sushidsp.
- Colour: handled entirely in sushicore (D:/Projects/sushicore/sushicore/config.py:104-113): [cli] color = auto|always|never, a prefixed *_COLOR env var, and NO_COLOR wins. `sd` has no --no-color/--color flag of its own.
- Help wording register is mixed: third person in configure/build/test/doxygen/check/run ("Runs", "Configures", "Builds", "Generates"), imperative in config/env ("Print") and in the four sushicore commands ("Provision", "Report", "Record", "Remove").
- Layout: one cli.py holds every command; services/ holds diag.py and project.py. There is no commands/ package. Tracked CLI tests: 7 files under cli/tests.
- Sibling panel names for comparison: sr Toolchain/Project/Diagnostics/Container; sa Project/Models/Diagnostics; sb Project/Documentation/Diagnostics; st Project/Benchmark/Diagnostics; se Build/Applications/Assets/Add-ons/Environment; hub Workspace/Modules/Dependencies/Desktop app/Account.

### Corrections from review

- Facts say 'Tracked CLI tests: 7 files under cli/tests'; git ls-files shows 8 files there (conftest.py plus 7 test_*.py). The audit's own verification_output says 8. The total of 21 tracked files under cli/ is correct.
- F5 says only sd spells `env --all` with `-a`, listing sb/sa/sr as the long-only ones. sushitrack also has `--all`, `-a` (D:/Projects/sushitrack/cli/sushitrack_cli/cli.py:355); se, sr, sa, sb are long-only. The split is 2 against 4, not 1 against the rest.
- F2 states sushicore 0.6.0 would not provide sushicore.provision.commands. Nothing in the tree supports or refutes it: sushicore's tags stop at v0.4.0 and the audit cites no source. The version mismatch itself (README 0.6.0, pyproject >=0.7.0, sushicore checkout 0.7.0) is correct.
- The not_checked list says the SD_CMAKE_GENERATOR name was not verified. It is correct (sushicore/profile.py:36 maps generator -> CMAKE_GENERATOR). The same table has no entry for llvm_root, see missed.

### Findings

#### C1. [high] `sd build -D VAR=VALUE` is silently ignored on an already configured tree

Evidence: D:/Projects/sushidsp/cli/sushidsp/services/project.py:186-193: `defines` reach cmake only inside `if _needs_configure(...)`; when the tree is configured and the build type matches, `configure(build_type, defines)` is skipped and `_DRIVER.compile` runs without them. The help text (cli.py:73-75) and docs/guides/COMMAND_LINE_INTERFACE.md:20 promise the variable is passed through. No message is printed.

Recommendation: Force a configure whenever `defines` is non-empty (as `sd configure -D` does), or reject the flag with an error telling the user to run `sd configure -D`. Add a test for the configured-tree case.

Review: confirmed. cli/sushidsp/services/project.py:186-193: defines only enter through configure() inside `if _needs_configure(...)`; _DRIVER.compile gets none. cli.py:73-75 and the guide line 20 promise pass-through. Read from source, not run. Recommendation (force configure when defines is non-empty) is a one-place fix.

#### C2. [high] README states the wrong sushicore version

Evidence: D:/Projects/sushidsp/README.md:29 says `sd` "needs sushicore 0.6.0"; D:/Projects/sushidsp/cli/pyproject.toml:12 requires `sushicore>=0.7.0`, and cli.py:14 imports `sushicore.provision.commands`, which 0.6.0 would not satisfy per that floor.

Recommendation: Change the README to 0.7.0, or drop the number and point at cli/pyproject.toml so it cannot drift again.

Review: confirmed. README.md:29 says 0.6.0; cli/pyproject.toml:12 says sushicore>=0.7.0; docs/reference/CHANGELOG.md:5 records the move to 0.7.0. The clause that 0.6.0 lacks provision.commands was not verifiable (sushicore tags stop at v0.4.0). Severity is closer to medium: a stale number in a README, not broken behaviour.

#### C3. [medium] No `--version` and no `--describe`; the root flags differ across the Sushi CLIs

Evidence: D:/Projects/sushidsp/cli/sushidsp/cli.py:29-36 defines no app callback at all. se defines `--version` (D:/Projects/sushiengine/cli/sushiengine/cli.py:114), hub defines `--describe` (D:/Projects/sushistack/cli/sushihub/cli.py:57). sr, sa, sb, st have neither (grep for `--version`/`--describe` in their cli.py returned nothing).

Recommendation: Put one root callback in sushicore (version lines plus a machine-readable --describe) and register it from every CLI, the way register_provision_commands is registered.

Review: confirmed. cli.py:29-36 has no callback; se cli.py:114 has --version, hub cli.py:57 has --describe; my grep over sr/sa/sb/st cli.py found neither flag. A shared root callback in sushicore is brick-shaped, but it is a public interface and so an owner decision.

#### C4. [medium] Help panel names differ from the sibling module CLIs

Evidence: sd uses Build / Application / Environment (cli.py:23-25). sr, sa, sb, st use Project / Diagnostics (e.g. D:/Projects/sushiblas/cli/sushiblas/cli.py:21-23), so `config` and `env` sit under "Diagnostics" there and under "Environment" here; se uses Build / Applications (plural) / Environment (sushiengine/cli/sushiengine/cli.py:41-45). The panel for setup/doctor/link/unlink is whatever each CLI passes as `panel=`.

Recommendation: Fix one panel vocabulary in sushicore as constants and have every CLI import it; the owner decides which set wins.

Review: confirmed. K_ constants recomputed in all seven cli.py files; they match the audit's list (sd Build/Application/Environment, se Applications plural, sr/sa/sb/st Project/Diagnostics).

#### C5. [medium] Command set and option set diverge from sa/sb/sr for the same jobs

Evidence: sd has no `clean` command (sr, sa, sb, se, st all have one; sd only offers `--clean` on build/test). sd `test` lacks `--filter/-f` and `--repeat/-r`, which sb, sa, sr, se, st all carry (sushiblas/cli/sushiblas/cli.py:83-88). sd `test` takes `--type`, sb's does not. sd `run` takes no target and launches one hard-coded binary, while sb `run` takes a target and `--sort`. sd alone has a `configure` command. sd `env --all` has `-a`; sb/sa/sr spell it without the short form (option census shows `"--all"` only).

Recommendation: Write the common build-command contract once (build, test, run, clean, docs; shared option names and short flags) and bring sd onto it: add `clean`, `--filter`, `--repeat`; decide whether `configure` belongs to all CLIs or none.

Review: confirmed. Command lists recomputed: sr, se, sa, sb, st each define `clean`; none but sd defines `configure`; --filter/-f and --repeat/-r are in sr, se, sa, sb, st; sb test has no --type; sb run takes target and --sort. One omission: st also spells `--all` with `-a` (sushitrack_cli/cli.py:355), so sd is not alone there.

#### C6. [medium] `sd check` has a different shape from `se check`

Evidence: sd: `sd check --report/-r --rule <name>`, a fixed list of three scripts (project.py:248-252). se: `se check [NAMES]... --list`, a registry (D:/Projects/sushiengine/cli/sushiengine/cli.py:890-908). sr, sa, sb, st have no `check`. sd's `--rule` is forwarded to all three checkers, and tools/common/checker.py:69 declares it with `choices=sorted(checker.rules)`, so a rule that belongs to one checker makes the other two exit with an argparse usage error and `sd check --rule X` returns non-zero.

Recommendation: Move one check runner into sushicore (registry of name -> script, `--list`, positional names) and use it in every CLI. Until then, send `--rule` only to the checker that owns the rule.

Review: confirmed. project.py:248-263 forwards --rule to all three scripts; all three import common.checker (check_docs_layout.py:20, check_changelog.py:18, check_layering.py:18) and tools/common/checker.py:69 uses choices=sorted(checker.rules). se check with --list at sushiengine cli.py:890-903. Whether rule names overlap between checkers was not compared, so the exit-2 claim holds for any rule owned by one checker only.

#### C7. [medium] `sd check` does not run the source-comment checker

Evidence: project.py:248-252 lists check_docs_layout.py, check_changelog.py, check_layering.py. D:/Projects/sushidsp/tools/documentation/check_source_comments.py exists and is not in the list. The PR template requires "`sd check` exits 0" (.github/PULL_REQUEST_TEMPLATE.md:22), so comment-shape violations pass the gate.

Recommendation: Add check_source_comments.py to the list (or to the shared registry from F6) and update the help line and README.md:35.

Review: confirmed. tools/documentation/check_source_comments.py is tracked and uses the same Checker frame (line 19); project.py:248-252 omits it; .github/PULL_REQUEST_TEMPLATE.md:22 gates on `sd check`; README.md:35 lists three checks.

#### C8. [medium] `sd check` falls back to a path that only exists in an editable install

Evidence: project.py:243-246: `except (SystemExit, Exception): root = Path(__file__).resolve().parents[3]`. Under pipx or a wheel install that resolves to the venv's lib directory, the three checker paths do not exist, and Python prints "can't open file" three times with exit 2 instead of the one-line "Not inside a SushiDSP project" message every other command gives. The clause also breaks the python-code-style rule against `except Exception` swallowing.

Recommendation: Drop the fallback and let find_project_root's SystemExit message through, like the other commands.

Review: confirmed. project.py:243-246 as quoted; parents[3] of cli/sushidsp/services/project.py is the repo root only in an editable install. The `except (SystemExit, Exception)` clause is real. Dropping the fallback is the right fix.

#### C9. [medium] `sd build --clean` / `sd test --clean` can end in a traceback on Windows

Evidence: project.py:119-125 `_remove_tree` calls shutil.rmtree with an onerror hook that chmods and retries once; a file held open (a running sushidsp_host_gui.exe, an open .pdb) raises PermissionError from the retry, and nothing between there and Typer catches it. sushicore's own CMakeDriver.clean_tree (D:/Projects/sushicore/sushicore/cmake_driver.py:134-141) removes with ignore_errors and reports.

Recommendation: Catch OSError around the removal, print one console.error line naming the locked path, return non-zero. Better: give CMakeDriver.clean_tree the read-only handling and call it here.

Review: confirmed. project.py:119-125: the onerror hook retries once and a second failure propagates; no caller catches OSError. sushicore cmake_driver.py:134-141 uses ignore_errors=True. Caveat on the recommendation: clean_tree prints 'removed' even when ignore_errors hid a failure, so moving the read-only handling there must also fix that message. Read from source, not run.

#### C10. [medium] `sd run` bypasses the shared Runner and reports differently from every other command

Evidence: project.py:309-320: a missing binary is reported with `console.header(f"Executable {exe_name} not found")` rather than console.error; the launch uses bare `subprocess.run` instead of `_RUNNER.run`, so the command line is not echoed; Ctrl+C returns 0, while sushicore.proc.Runner returns 130 for an interrupt (D:/Projects/sushicore/sushicore/proc.py:84-87).

Recommendation: Launch through `_RUNNER` (constructed with catch_interrupt=True) and report the missing binary with console.error.

Review: confirmed. project.py:309-320: console.header for the missing binary, bare subprocess.run, KeyboardInterrupt returns 0. Runner._interrupted returns 130 (proc.py:84-87) only when built with catch_interrupt=True, which the audit's recommendation states. `check` (project.py:261) bypasses the Runner the same way and the audit does not say so.

#### C11. [medium] `target_bin` / `default_target` are dead configuration; `sd run` hard-codes the binary

Evidence: D:/Projects/sushidsp/cli/config.toml:22-23 sets `target_bin = "sushidsp_host"` with the comment "this CLI's future `sd run`"; cli/sushidsp/config.py:41 and :72 repeat it as `default_target` and `Config.target_bin`; it is listed as an SD_* env field (config.py:45). project.py:286 launches `sushidsp_host_gui` regardless. `sd config` therefore shows a value that changes nothing.

Recommendation: Either make `run` honour target_bin (and take an optional target like sb/sa) or delete the field, the env override and the config.toml entry.

Review: confirmed. cli/config.toml:22-23, config.py:41, :45, :72 as quoted; project.py:286 hard-codes sushidsp_host_gui. target_bin is read nowhere in cli/sushidsp.

#### C12. [medium] Clang without Ninja on Windows: the code and guide assume a single-config default that CMake does not pick

Evidence: config.py:140-158: with a compiler found and no ninja, resolved_generator() returns "" and is_multi_config() returns True on Windows, yet project.py:156-160 still passes -DCMAKE_CXX_COMPILER. The docstring (config.py:143-147) and docs/guides/COMMAND_LINE_INTERFACE.md:117-120 say "CMake's single-config default (Makefiles or NMake) carries the compiler". On a Windows machine with Visual Studio installed CMake's default generator is Visual Studio, which ignores CMAKE_CXX_COMPILER, so the banner "Compiler: clang++" names a compiler the build does not use. Read from source, not run.

Recommendation: On Windows with a resolved compiler and no ninja, either stop with an error asking for ninja or drop the compiler and report the platform default; correct the guide either way.

Review: confirmed. config.py:140-158 and project.py:156-160 as quoted; the code contradicts itself: is_multi_config() treats '' on Windows as Visual Studio while the docstring and guide lines 117-120 call it single-config. Related and unmentioned: _generator_changed (project.py:114-115) wipes the tree on every build when the cached default generator is NMake Makefiles. Runtime behaviour of CMake's default generator not run.

#### C13. [medium] `sd run --type` launches whatever binary is already in the tree and labels it with the requested type

Evidence: D:/Projects/sushidsp/cli/sushidsp/services/project.py:289-313: _locate() tries build/apps/host_gui/<exe> and build/bin/<exe> before the per-config folders and never compares the tree's CMAKE_BUILD_TYPE with the requested one. On a single-config (Ninja) Debug tree, `sd run --type release` finds the Debug binary, skips the build, and prints 'Launching sushidsp_host_gui.exe (Release)'. Because the build only runs when no binary is found, `sd run` also launches a stale binary after source edits. Read from source, not run.

Recommendation: Have run() call build(bt) first, as test() does; build already reconfigures on a type change and is a no-op when up to date. Then locate the binary for that type only.

Review: found by review.

#### C14. [medium] `llvm_root` is declared as an SD_* environment field but no variable is derived for it

Evidence: D:/Projects/sushidsp/cli/sushidsp/config.py:44-45 lists llvm_root in PROFILE.env_fields. D:/Projects/sushicore/sushicore/profile.py:34-47 (_TOOL_ENV_SUFFIXES) has no llvm_root key, and env_overrides() (profile.py:96-103) only emits fields found in that table; PROFILE passes no extra_env_overrides. So SD_LLVM_ROOT does not exist, while the config.py docstring (lines 7-8) and the guide's precedence section say every value resolves through SD_* variables. The audit listed this table as not checked.

Recommendation: Add llvm_root to the shared suffix table in sushicore (it is a ProvisionSettings field every CLI carries), or remove it from sd's env_fields. Have env_overrides() reject an env_fields name it cannot map, so the next one fails loudly.

Review: found by review.

#### C15. [medium] `sd check` also bypasses the shared Runner, and its `-r` means something else than `-r` on sibling commands

Evidence: project.py:256-263 runs each checker with bare subprocess.run: no command echo, no header between checkers, no interrupt handling, the same defect F10 reports for `run` only. cli.py:144 gives `--report` the short flag `-r`; in sr, se, sa, sb and st `-r` is `--repeat` (for example sushiblas/cli/sushiblas/cli.py:86), and `se check` has no short flags.

Recommendation: Fold this into the shared check runner proposed in F6: it runs through Runner and reserves short flags from one table in sushicore.

Review: found by review.

#### C16. [medium] cli/ has no README of its own

Evidence: `git ls-files cli` in D:/Projects/sushidsp lists no cli/README.md, and `ls cli/README.md` fails. sushiengine carries cli/README.md; sr, sa, sb, st do not. The documentation architecture puts a module's own facts in <module>/README.md beside the code; for sd those facts (install with pip -e, config layering, where config.local.toml lives) are split between README.md:26-36, cli/config.toml comments and the guide.

Recommendation: Add cli/README.md with the install line, the sushicore floor read from pyproject.toml, and a link to docs/guides/COMMAND_LINE_INTERFACE.md; do the same in the four siblings so the six module CLIs are shaped alike.

Review: found by review.

#### C17. [low] Local ctest invocation duplicates CMakeDriver.ctest_run on a stale premise

Evidence: project.py:207-221 builds the ctest command by hand; the comment says CMakeDriver.ctest_run "has no -C <config> knob". It has one: `config: str | None` at D:/Projects/sushicore/sushicore/cmake_driver.py:103-130. The remaining difference is drained versus inherited output and the `-V` for the benchmark suite.

Recommendation: Call `_DRIVER.ctest_run(..., label_regex=..., config=...)`; if `-V` or inherited stdout is needed, add that parameter to the driver so the filter/repeat options of F5 come for free.

Review: confirmed. sushicore cmake_driver.py:103-130: ctest_run takes `config` and passes -C; the comment at project.py:207-210 is stale on that point. The drained-versus-inherited and -V differences are real, as the audit says.

#### C18. [low] `_DOXYGEN_HINT` copies sushicore's default hint

Evidence: project.py:93-97 against D:/Projects/sushicore/sushicore/cmake_driver.py:19-23: four identical lines except `cli/config.local.toml` versus `config.local.toml`.

Recommendation: Drop the local constant and use the driver's default, or have sushicore build the hint from the module's config directory.

Review: confirmed. project.py:93-97 against cmake_driver.py:19-23: identical except `cli/config.local.toml`. The local text is the more accurate path, so the second option (sushicore builds the hint from the module's config directory) is the better brick.

#### C19. [low] Doxygen output path is stated three different ways

Evidence: Doxyfile:4-5 writes to build/docs/api-site/html. The `sd doxygen` help says "(docs/api-site/html)" (cli.py:132); docs/getting_started/INTRODUCTION.md:24 says "under docs/api-site/html/"; .gitignore ignores docs/api-site/html/. docs/guides/COMMAND_LINE_INTERFACE.md:49 and docs/README.md:41 say build/docs/api-site/html/, which matches the Doxyfile.

Recommendation: Correct cli.py:132 and INTRODUCTION.md:24 to build/docs/api-site/html; remove the stale .gitignore lines.

Review: confirmed. Doxyfile:4-5, cli.py:132, docs/getting_started/INTRODUCTION.md:24, .gitignore:14-15, guide line 49 and docs/README.md:41 all read as quoted.

#### C20. [low] Compiler-search labels in the CLI guide no longer match the code

Evidence: docs/guides/COMMAND_LINE_INTERFACE.md:105-111 says step 2 is reported as `SushiStack workspace (intel/llvm)` and probes only `<workspace>/dependencies/toolchains/...`. config.py:83-96 has three origins: `llvm_root (intel/llvm)`, `provision root <root> (intel/llvm)` from sushicore.provision.home.search_roots(), and the workspace one, in that order. The provision-root probe, which is where `sd setup --toolchain intel-llvm` installs, is not described.

Recommendation: Rewrite step 2 of the guide with the three origins and their labels.

Review: confirmed. Guide lines 105-111 against config.py:83-96: three origins with three labels in code, one label and no provision-root probe in the guide. The guide also names a `ToolConfig` class at line 33 that sd does not have (Config / ProvisionSettings).

#### C21. [low] Stale statements in CLI docstrings, config comments and contributor docs

Evidence: project.py:5-9 says the CLI inherits "the parent environment unchanged: SushiDSP needs no vcvars snapshot", while cli/sushidsp/env.py:58-81 loads exactly that snapshot and project.py:100-101 uses it. config.toml:22 calls `sd run` "future". docs/CONTRIBUTING.md:106 says "`sd test --suite all` once more than `unit` exists" although regression and benchmark exist. docs/README.md:14 advertises "presets", which the guide and CLI do not have. .github/workflows/ci.yml:52 calls `-L "unit|regression"` "the umbrella label regex `sd test --suite all` uses", but `all` passes no -L and also runs the benchmark case (project.py:52-56, 217-219). resolve_build_type's `except Exception` (project.py:82-85) can never fire, since find_project_root raises SystemExit.

Recommendation: Fix each line in the same change that touches its file; remove the dead except clause.

Review: confirmed. Each item opened: project.py:5-9 against env.py:58-81; config.toml:22; docs/CONTRIBUTING.md:106; docs/README.md:14 'presets'; ci.yml:52; project.py:82-85 (find_project_root raises SystemExit, module_config.py:75, which `except Exception` does not catch).

#### C22. [low] Help strings mix two grammatical registers

Evidence: cli.py:55 "Runs cmake's configure step", :77 "Configures (if needed) and builds", :99 "Builds (if needed) and runs ctest", :132 "Generates", :147 "Runs", :166 "Runs" against :109 "Print the resolved configuration", :122 "Print the environment", and the sushicore commands "Provision", "Report", "Record", "Remove" (D:/Projects/sushicore/sushicore/provision/commands.py:274, 284, 301, 322). sb/sa use the imperative throughout.

Recommendation: Pick one form for command summaries across all CLIs (the imperative is what sushicore and four siblings already use) and rewrite the six sd lines.

Review: confirmed. cli.py:55, 77, 99, 132, 147, 166 are third person; :109, :122 and sushicore provision/commands.py:274, 284, 301, 322 are imperative; sb cli.py uses 'Run'.

#### C23. [low] The `--type` help text is written four times and `test` has no `--define`

Evidence: cli.py:47-50, 67-70, 89-92, 159-162 repeat the same two-sentence help literal. `test` (cli.py:88-100) builds through project_svc.build but offers no `--define`, so a test run cannot carry the cache variable that `build` accepts.

Recommendation: Declare the option once as a module-level Annotated type and reuse it; decide whether `test` forwards `--define` (after F1 is fixed).

Review: confirmed. The --type literal is repeated at cli.py:47-50, 67-70, 89-92, 159-162; test (cli.py:88-100) has no --define and project.test calls build() without defines. One Annotated alias is a brick-shaped fix.

#### C24. [low] The shared `setup` help shows SYCL examples in a CLI that compiles no SYCL

Evidence: D:/Projects/sushicore/sushicore/provision/commands.py:261-262 hard-codes the epilog "setup --toolchain adaptivecpp  # Also install a second SYCL toolchain" for every program, so `sd setup --help` prints it. cli/sushistack.deps.toml:25 states "SushiDSP compiles no SYCL", and the guide's own example is `--toolchain intel-llvm`.

Recommendation: Let ModuleProvision supply the setup examples (or derive the example toolchain from the module's fragment).

Review: confirmed. sushicore provision/commands.py:261-262 hard-codes the adaptivecpp SYCL example for every program; cli/sushistack.deps.toml:25 says SushiDSP compiles no SYCL.

#### C25. [low] A malformed config.local.toml appears to reach the user as a traceback

Evidence: No `except` exists in D:/Projects/sushicore/sushicore/config_base.py (grep for `except|TOMLDecodeError` returned nothing there), and cli/sushidsp has no handler around load_config (config.py:161-163; every service calls it bare). Inferred from source; not run.

Recommendation: Catch the TOML decode error in sushicore's loader, re-raise as a sushicore config error naming the file and line, and have each CLI entry point print it as one line.

Review: confirmed. sushicore workspace.py:42-47 read_toml calls tomllib.load with no handler; config_base.py and module_config.py have no TOMLDecodeError handler (the only one, workspace.py:238, belongs to another function); cli/sushidsp has none. Source-level only, as the audit states.

### Not checked

- Nothing was executed: no `sd` command, no help screen, no test. Every statement about runtime behaviour (F1, F6, F8, F9, F12, F21) is read from source.
- sushicore internals beyond the files read: diag.py (config_show/env_dump bodies), cli_console.py, console.py, renderer.py, theme.py, config_base.py, workspace.py, the provision pipeline, steps, packages and toolchains.
- The mapping `_TOOL_ENV_SUFFIXES` in sushicore/profile.py, so the `SD_CMAKE_GENERATOR` name in cli/config.toml:19 and the full list of SD_* variables were not verified.
- Whether cli/sushidsp/env.py (cache key, toolchain PATH injection) duplicates the env modules of sr/se/sa/sb; those files were not opened.
- The seven test files under cli/tests were listed by test name only (test_help_screen.py, test_provision_commands.py); their bodies and coverage were not read.
- tools/ checker bodies (only the argparse lines of tools/common/checker.py were read); whether each checker's rule names overlap.
- Sibling CLIs were sampled by grep for command names, options, panels and scripts; only sushiblas cli.py:60-160 and sushiengine cli.py:888-915 were read in full. Sibling services were not read.
- docs/design/*, docs/agent/*, docs/archive/*, docs/reference/CHANGELOG.md and CLAUDE.md mentions of `sd` were not compared against the code; only README.md, docs/guides/COMMAND_LINE_INTERFACE.md, docs/CONTRIBUTING.md (lines 24-40, 100-110), docs/README.md, docs/getting_started/INTRODUCTION.md (grep lines) and ci.yml (lines 46-56) were.
- sushi-module.toml and `hub install-cli sushidsp` behaviour (only confirmed that hub defines `install-cli`).
- Python style conformance of the CLI (commands/ package, services writing to the console, `Optional` versus `X | None`, requires-python 3.10 versus the 3.11 rule, missing docstrings on BuildType/Suite/_build_dir/_env) was noticed but left to the code-review dimension.
- Licence headers in cli/ (currently Apache-2.0 in every file read) belong to the licence dimension.

## Documentation

### Facts

- Present: README.md, docs/README.md, docs/CONTRIBUTING.md, docs/DOCUMENTATION_STYLE_GUIDE.md, docs/getting_started/ (INTRODUCTION.md only), docs/architecture/ (README, OVERVIEW, MODULES), docs/guides/ (COMMAND_LINE_INTERFACE.md only), docs/reference/ with CHANGELOG.md, GLOSSARY.md, KNOWN_ISSUES.md, API_REFERENCE.md and spice/.
- Present: docs/design/ with README.md (topic map), REMAINING_WORK.md and four topic documents (SUSHIDSP, GUI_UX_REDESIGN, ROOM_FURNISHINGS, SUSHI_STANDARDS_MIGRATION); docs/archive/ with README.md.
- Missing: docs/modules/ (the global CLAUDE.md tree lists it; the repo keeps the module index at docs/architecture/MODULES.md instead, which matches the documentation skill's tree, where modules/ is absent).
- docs/agent/ uses the skill's per-work-folder shape (2026_09_15_SUSHI_STANDARDS_PHASE_1, 2026_09_20_V0_1_0_RELEASE, 2026_09_22_ROOM_FURNISHINGS, each holding only SPEC/PLAN/REPORT), not the specs|plans|reports split named in the global CLAUDE.md. The two yardsticks disagree here; the repo follows the skill.
- Module READMEs present for all 12 modules under modules/nodes, modules/catalog, modules/session, plus apps/host_gui, assets/hrtf, assets/ir, tools, tools/dsp, tools/spice, tests/golden, third_party/dr_libs.
- No README.md in: apps/host, apps/host_asio, cli, tests, tests/measure, tests/benchmark, tools/documentation, tools/layering, and every include/SushiDSP/* directory (core, math, math/dk, circuits, device, io, audio_format).
- No place in the architecture: docs/archive/superpowers/ (plans/ 29 files, specs/ 14 files), docs/archive/research/ (7 files), docs/archive/notes/ (2 files). All are finished agent or research work, so archive is the right bucket by question 4, but the superpowers/ name and dated lower-case file names (2026-08-25-asio-backend.md, *.ledger.md) break the UPPER_SNAKE_CASE and work-folder naming.
- docs/reference/spice/ (8 sub-READMEs plus netlists, 40 tracked files) is an extra folder under reference/; it is reference data, so the manual is its bucket by question 5, but each sub-README is a fact about one module (jcm800, tubescreamer) by question 1.
- Root carries LICENSE and NOTICE once each; no duplicated CHANGELOG or LICENSE under docs/. third_party/dr_libs/LICENSE.txt is the vendored library's own. No docs/superpowers, docs/slop, docs/api or docs/api-site exists in the tree.
- Single backlog confirmed: docs/design/REMAINING_WORK.md. No '- [ ]' checklists and no TODO/FIXME in live design docs, agent folders or module READMEs.
- Changelog: 346 entries, none over 240 characters, no nested bullets, no continuation lines, no entry with a second sentence, none with more than five cited places. Headings are '## Unreleased' and '## v0.1.0 — 2026-10-03' only.
- CLI: cli/sushidsp/cli.py defines configure, build, test, config, env, doxygen, check, run and registers sushicore's provision commands (setup, doctor, link, unlink per the guide); the guide's count of twelve subcommands is consistent with that.
- Link check over 62 tracked non-archive Markdown files plus the archive: 25 Markdown links do not resolve, all 25 in module READMEs. Backticked-path check on live docs: 52 cited paths do not resolve.
- Root README.md has zero em dashes in 774 words and reads in register; CONTRIBUTING.md has 26 in 1094 words, COMMAND_LINE_INTERFACE.md 28 in 884, API_REFERENCE.md 9 in 204, OVERVIEW.md 11 in 729.

### Corrections from review

- docs/archive/superpowers/plans holds 30 tracked files, not 29. The archive is 54 tracked Markdown files including its README, 53 without; the audit's '52 files except its README' is one short.
- docs/reference/spice holds 36 tracked files (9 README.md: the top one plus 8 sub-READMEs), not 40.
- F7: the unreachable count is 68 documents (71 with CLAUDE.md, AGENTS.md, .github/PULL_REQUEST_TEMPLATE.md), not 62; the audit's own enumeration sums to 66. Not all seven docs/agent files are unreached: 2026_09_20_V0_1_0_RELEASE/PLAN.md and REPORT.md and 2026_09_22_ROOM_FURNISHINGS/PLAN.md are reached through design documents, and six archive plans/specs are reached the same way (47 archive files unreached, not 52).
- F7: the eight docs/reference/spice/*/README.md and tools/dsp, tools/spice READMEs are not 'reached only through a bare directory link'; they are not reached at all, because docs/reference/spice/README.md and tools/README.md carry no link to them.
- F5: changelog entries without a backticked place are 7 (lines 115, 124, 240, 245, 250, 300, 301), not 5. Reason clauses are at lines 10, 14, 19, 33, 41, not 10, 13, 18, 31, 38. The first out-of-order step is at line 41, not 38.
- Facts list says 62 tracked non-archive Markdown files; git ls-files gives 61 outside third_party/ (62 only with third_party/dr_libs/README.md).
- F4: 'its own entry contradicts the file' overstates it. Commit c08d594 folded the headings into Unreleased and commit de235ac 'chore(release): v0.1.0' renamed the section afterwards, so CHANGELOG.md:9 records a step that did happen. The defects that remain are the missing tag and the cited docs/archive/changelog/ that does not exist.

### Findings

#### D1. [high] Every module README's links to docs/ are one level short and do not resolve

Evidence: 25 broken links across all 12 module READMEs under D:/Projects/sushidsp/modules/{nodes,catalog,session}/*/README.md. They use '../../docs/...' but the modules now sit three levels deep, so the target resolves to modules/docs/. Examples: modules/nodes/jcm800/README.md -> ../../docs/design/SUSHIDSP.md; modules/nodes/room/README.md -> ../../docs/reference/KNOWN_ISSUES.md (3 times) and 2 archive links; modules/nodes/capture/README.md (3); modules/nodes/diezel_vh4/README.md (3); modules/nodes/tubescreamer/README.md (3); modules/catalog/node_registry/README.md; modules/session/rig_preset/README.md. Caused by the 2026-09-21 regrouping (CHANGELOG line 'Grouped the twelve modules into the nodes, catalog and session tiers').

Recommendation: Change '../../docs/' to '../../../docs/' in the 12 module READMEs and have check_docs_layout.py's link rule cover module READMEs so the next move fails the check.

Review: confirmed. Recomputed the link check: 25 broken links, all in the 12 module READMEs (room 5; capture, diezel_vh4, jcm800, tubescreamer 3 each; dd3 2; the rest 1). CHANGELOG.md:46 carries the regrouping entry. SOLID note on the recommendation: tools/README.md is byte-identical to D:/Projects/sushiskills/tools/README.md and says a rule is fixed in the reference first and copied out, so the link-rule change belongs in sushiskills' check_docs_layout.py, not in a local edit of this repo's copy.

#### D2. [high] Module READMEs and design docs still name the pre-regrouping paths modules/<name>

Evidence: Line 1 of all 12 module READMEs titles the module with the old path (e.g. modules/nodes/jcm800/README.md:1 cites `modules/jcm800`, modules/catalog/node_registry/README.md:1 `modules/node_registry` and :47 `modules/rig_preset`; modules/nodes/cabinet/README.md:9-10 cites modules/jcm800, peavey5150, diezel_vh4, mesa_mark2c). Same in docs/design/GUI_UX_REDESIGN.md:177,184,185 and docs/design/SUSHIDSP.md:218,311,605,816,817,835,858. None of those directories exist.

Recommendation: Repoint to modules/nodes/<name>, modules/catalog/node_registry, modules/session/rig_preset in the READMEs and the two design documents.

Review: confirmed. Line 1 of all 12 module READMEs names modules/<name>; node_registry/README.md:47, cabinet/README.md:9-10, GUI_UX_REDESIGN.md:177,184,185 and SUSHIDSP.md:218,311,605,816,817,835,858 checked, none of the directories exist.

#### D3. [high] Root README states the wrong sushicore requirement

Evidence: D:/Projects/sushidsp/README.md 'Build and run': 'needs sushicore 0.6.0, which is not on PyPI yet'. D:/Projects/sushidsp/cli/pyproject.toml declares 'sushicore>=0.7.0', and CHANGELOG.md:5 (2026-10-04) records 'required sushicore 0.7.0'. .github/workflows/ci.yml:91 comment also still says 0.6.0.

Recommendation: Update README and the CI comment to 0.7.0 and restate the PyPI availability from the actual state of the sushicore release.

Review: confirmed. README.md:29 says sushicore 0.6.0; cli/pyproject.toml:12 says sushicore>=0.7.0; .github/workflows/ci.yml:91 says 0.6.0; CHANGELOG.md:5 records 0.7.0. D:/Projects/sushicore/pyproject.toml is at 0.7.0.

#### D4. [high] Changelog heading claims a v0.1.0 release that has no tag, and its own entry contradicts the file

Evidence: D:/Projects/sushidsp/docs/reference/CHANGELOG.md:7 '## v0.1.0 — 2026-10-03' holds 345 entries; 'git tag' prints nothing, and the skill says the heading is the tag. Line 9 says 'Folded the untagged v0.1.0, v0.2.0 and v0.3.0 headings into Unreleased (`CHANGELOG.md`, `docs/archive/changelog/`)', but the entries sit under v0.1.0, not Unreleased, and docs/archive/changelog/ does not exist.

Recommendation: Decide with the owner whether v0.1.0 is tagged. If not, rename the section back to Unreleased; if yes, tag it. Correct or drop the cited docs/archive/changelog/ path.

Review: confirmed. CHANGELOG.md:7 is '## v0.1.0 — 2026-10-03', git tag prints nothing, docs/archive/changelog/ is absent. Nuance: commit de235ac 'chore(release): v0.1.0' exists after c08d594 (the fold), so line 9 is a true record of an earlier step and not a contradiction; the real defect is a release commit without a tag plus the dead cited path.

#### D5. [medium] Changelog entries that break the entry shape

Evidence: docs/reference/CHANGELOG.md: no 'where' in backticks at lines 115, 124, 240, 245, 250 (e.g. 124 'docs: Corrected room wall-clock figures measured on debug binaries.'); present tense at line 190 ('capture: Folds mono track playback...'); entries carrying the reason: line 10 ('which cannot install sushicore 0.6.0 from PyPI'), 13 ('instead of exhausting memory'), 18 ('so it no longer runs ahead of the wet'), 31 ('which the backlog had been asking for'), 38 ('the shape every reader understands'); order not newest-first at two places (2026-09-22 entries at lines 38 and 47 below 2026-09-21 entries); scopes 'host' and 'host_gui' both used for apps/host_gui. Cited places that no longer exist: line 34 cli/sushidsp/services/setup.py, 181 apps/host_gui/ui/TrackPanel.cpp, 304 include/SushiDSP/math/ResamplerCoefficients.hpp, 87 and 332 modules/<name>.

Recommendation: Fix the six shape defects and the ordering in the still-live section; strip the reason clauses. Leave historical file names alone if the owner treats released entries as frozen, but then move them to the archive as the skill prescribes.

Review: confirmed. Defects are real but several line numbers are off: the reason clauses sit at 10, 14, 19, 33 and 41 (not 13, 18, 31, 38); the order breaks are at 41 and 47 (not 38 and 47); entries with no backticked place are 7, not 5 (115, 124, 240, 245, 250, 300, 301). Line 190 'Folds', line 34 setup.py, 181 TrackPanel.cpp, 304 ResamplerCoefficients.hpp, 87 and 332 old module paths all check out.

#### D6. [medium] docs/archive/README.md says the archive is empty; it holds 54 files

Evidence: D:/Projects/sushidsp/docs/archive/README.md: 'SushiDSP hasn't closed out a design document yet, so this directory is currently empty'. The directory holds notes/ (2), research/ (7), superpowers/plans (29) and superpowers/specs (14). The README also links none of them.

Recommendation: Rewrite the archive README as an index of what is there (it is the one file in archive that is not frozen content).

Review: confirmed. docs/archive/README.md says the directory is 'currently empty'; git ls-files shows 53 tracked files beside it (notes 2, research 7, superpowers/plans 30, superpowers/specs 14). The README links none.

#### D7. [medium] 62 documents are not reachable from docs/README.md

Evidence: Transitive link walk from D:/Projects/sushidsp/docs/README.md. Not reached: docs/design/README.md and docs/design/SUSHI_STANDARDS_MIGRATION.md (the index links three design docs directly and skips the map and this one); all 7 files under docs/agent/; all 52 files under docs/archive/ except its README; apps/host_gui/README.md; assets/ir/README.md; tests/golden/README.md; tests/fixtures/bras_cr2_reference.README.md and motus_reference.README.md. The 8 docs/reference/spice/*/README.md and tools/dsp, tools/spice READMEs are reached only through a bare directory link. Root README.md claims 'every document in the tree is reachable from it'.

Recommendation: Link docs/design/README.md from the index and let it carry all four topics; add a docs/agent entry; index the archive from archive/README.md; add the host_gui, assets/ir, tests/golden and fixtures READMEs.

Review: confirmed. The defect is real, the count is not. A transitive walk from docs/README.md, resolving directory links to their README, leaves 68 documents unreached (71 with CLAUDE.md, AGENTS.md and the PR template). The audit's own list sums to 66, not 62. Three of the seven docs/agent files and six archive files are reached through design documents; the eight spice sub-READMEs and tools/dsp, tools/spice are not reached at all, since docs/reference/spice/README.md and tools/README.md link none of them.

#### D8. [medium] DOCUMENTATION_STYLE_GUIDE.md describes a tree that does not exist

Evidence: D:/Projects/sushidsp/docs/DOCUMENTATION_STYLE_GUIDE.md 'Where a document goes' lists docs/modules/, docs/api/, docs/api-site/, docs/superpowers/specs and docs/superpowers/plans; none exist (API page is docs/reference/API_REFERENCE.md, Doxygen output is build/docs/api-site per Doxyfile:4, superpowers is under docs/archive/). It also says CHANGELOG is grouped 'by roadmap phase', shows status-line examples ('**Status:** Design — open phase, nothing implemented yet.') that differ from the skill's 'Open — phase N of M' / 'Shipped', shows a changelog example with no scope, and uses 'modules/<name>/' for the flat layout. Its Voice section says 'Explain why, never what' for commit messages and source comments, the opposite of the global source-comment rule.

Recommendation: Rewrite the tree block from the actual tree, align status and changelog examples with the documentation skill, and remove the why-not-what rule.

Review: confirmed. DOCUMENTATION_STYLE_GUIDE.md:8-33 tree lists docs/modules/, docs/api/, docs/api-site/, docs/superpowers/; line 21 'by roadmap phase' (line 97 of the same file says 'by release'); status examples at 81 and 85; changelog example at line 100 has no scope; line 127 'Explain why, never what'.

#### D9. [medium] CONTRIBUTING.md contradicts the code, the other docs and the global rules

Evidence: D:/Projects/sushidsp/docs/CONTRIBUTING.md: section 1 sets up through 'hub add sushidsp' / 'hub install-cli sushidsp', while sushi-module.toml says 'SushiDSP is not in hub's catalog' and README says to pip install sushicore then './cli'; it mentions 'Linux/macOS' while README says 'Windows and Linux'. Section 3: constants 'kPascalCase' while docs/design/SUSHIDSP.md line 4 says 'UPPER_SNAKE'; 'Comments — none, unless the why is non-obvious' and '@brief on every public function' against the global rule (every symbol, never the reason). Section 4: 'Every test lives under tests/functional/unit/' while tests/functional/regression, tests/circuits, tests/measure and tests/benchmark exist. Section 6: commit messages 'Explain why, not what'. It never states what a change must document (README, design status, changelog line), which is this file's job in the architecture; 'see the WDF adaptor-tree deferral in the D2 plan' cites no path.

Recommendation: Rewrite sections 1, 3, 4 and 6 against the tree and the sushiskills rules, and add the same-commit documentation rule.

Review: confirmed. CONTRIBUTING.md:20 Linux/macOS, :28-29 hub add / hub install-cli, :53 'D2 plan' with no path, :65 kPascalCase (SUSHIDSP.md:4 says UPPER_SNAKE), :72-73 Doxygen on public functions only and comments for the why, :83 every test under tests/functional/unit (tests/functional/regression, circuits, measure, benchmark exist). The file never mentions the changelog.

#### D10. [medium] Architecture pages are stale against the module layout and omit the cabinet module

Evidence: D:/Projects/sushidsp/docs/architecture/OVERVIEW.md layer diagram shows 'modules/<product>/' flat and lists 11 modules without cabinet; the 'Products' section says 'the six product modules and routing/capture/room'. modules/nodes/cabinet/ exists with a README. docs/architecture/MODULES.md lists 11 modules, no cabinet, although docs/README.md links modules/nodes/cabinet/. MODULES.md also does not link include/SushiDSP/ or tools/ directories as docs/architecture/README.md promises ('links each include/SushiDSP/, modules/ and tools/ directory to its own README.md'); the include directories have no README at all. OVERVIEW.md says 'see docs/README.md for the per-header description' of math/dk, which the index does not carry.

Recommendation: Redraw the diagram with the nodes/catalog/session tiers, add cabinet to both pages, and either write the include/SushiDSP/* READMEs or correct architecture/README.md.

Review: confirmed. OVERVIEW.md:14-15 flat modules/<product>/ with 11 names and no cabinet; MODULES.md lists 11 modules, no cabinet, no include/ or tools/ link; architecture/README.md:6-7 promises those links; OVERVIEW.md:51 sends the reader to docs/README.md for a per-header description it does not hold.

#### D11. [medium] Getting-started page gives the wrong Doxygen output path and is too thin to start from

Evidence: D:/Projects/sushidsp/docs/getting_started/INTRODUCTION.md:24 'generates the API reference under docs/api-site/html/'; Doxyfile:4 sets OUTPUT_DIRECTORY = build/docs/api-site, and the guide, index and CLAUDE.md say build/docs/api-site/html/. The page is 30 lines, has no install step (sd setup, sd doctor, sushicore install), and defers setup to CONTRIBUTING.md section 1, which describes a hub flow the README does not (F9). docs/reference/API_REFERENCE.md describes docs/README.md as 'what the library is and how to build it', which it is not.

Recommendation: Fix the path and give the page the real first-run sequence: install sushicore, pip install -e ./cli, sd setup, sd doctor, sd build, sd test, sd run.

Review: confirmed. INTRODUCTION.md:24 says docs/api-site/html/, Doxyfile:4 says build/docs/api-site. Page has no install step. API_REFERENCE.md:11 describes docs/README.md as 'what the library is and how to build it'. Same stale path is in cli/sushidsp/cli.py:132 (the doxygen command's help text), which the audit did not list.

#### D12. [medium] Design spec and module manifest disagree on whether SushiDSP is in hub's catalog

Evidence: D:/Projects/sushidsp/docs/design/SUSHIDSP.md:894 (section 11a): 'SushiDSP is a registered SushiStack module, listed by that repository's own cli/sushistack/services/modules.py ... cloned with hub add sushidsp'. D:/Projects/sushidsp/sushi-module.toml: 'SushiDSP is not in hub's catalog, so this file is the only thing that makes it recognisable there'. The cited path cli/sushistack/services/modules.py does not exist in this repo and the sushistack package is now named sushihub.

Recommendation: Settle the fact with the owner and correct section 11a and CONTRIBUTING section 1 together.

Review: confirmed. SUSHIDSP.md:894 says registered, cloned with hub add sushidsp, cites cli/sushistack/services/modules.py; sushi-module.toml lines 1-2 say SushiDSP is not in hub's catalog.

#### D13. [medium] Design status lines disagree between the map and the documents, and one is malformed

Evidence: docs/design/README.md row: ROOM_FURNISHINGS 'Open — phase 4 of 4, the absorption calibration decision pending'; docs/design/ROOM_FURNISHINGS.md:2 '**Status:** Open — phase 4 of 4, the low-band built-room loss unresolved'. docs/design/REMAINING_WORK.md:3 '**Status:** open, last revised 2026-09-22.' (line 3, not the skill's form; 20 changelog days newer than the revision date). docs/design/SUSHI_STANDARDS_MIGRATION.md has its status on line 3, not line 2. All four topic documents do carry a status line.

Recommendation: Make the map copy each document's status verbatim and move the two status lines to line 2.

Review: confirmed. design/README.md row and ROOM_FURNISHINGS.md:2 differ in wording; REMAINING_WORK.md:3 '**Status:** open, last revised 2026-09-22.'; SUSHI_STANDARDS_MIGRATION.md status on line 3; SUSHIDSP.md and GUI_UX_REDESIGN.md carry it on line 2.

#### D14. [medium] README test count is not reproducible from the tree

Evidence: D:/Projects/sushidsp/README.md 'How the models are held in place': '1007 tests run under sd test'. A count of TEST/TEST_F/TEST_P macros across tests/, modules/ and apps/ gives 1120; the default suite is 'unit' per docs/guides/COMMAND_LINE_INTERFACE.md, so the 1007 figure may be the unit label only, but the sentence does not say so. 'Twelve of them are golden files' matches tests/golden (12 .f32); 'Thirty-nine tests' in test_rt_safety.cpp matches (39).

Recommendation: Have the owner run sd test once and restate the number with the suite it belongs to, or drop the hard count.

Review: confirmed. README.md:66 says 1007; a count of TEST/TEST_F/TEST_P lines over tests/, modules/, apps/ gives 1120; test_rt_safety.cpp has 39; tests/golden has 12 .f32 files. The real ctest count was not run, so the finding stands as 'not reproducible', not as 'wrong'.

#### D15. [medium] Other cited paths that do not resolve in live documents

Evidence: apps/host_gui/README.md:58 and assets/ir/README.md:3 cite assets/ir/default_4x12.wav (assets/ir holds only README.md); docs/design/SUSHI_STANDARDS_MIGRATION.md:36-37 cite docs/CLAUDE.md and docs/AGENTS.md (both are at the root) and :82 docs/api/mainpage.md; docs/architecture/OVERVIEW.md:81,82,90 and docs/design/SUSHIDSP.md:819 cite apps/plugin (stated as not built, acceptable as intent but written as a path). CLAUDE.md names the doctrine skill 'sushisystemsprogramming'; the installed skill is 'sushiskills'.

Recommendation: Correct or mark each as an example; state in assets/ir/README.md that the IR is not shipped if that is the case.

Review: confirmed. assets/ir holds only README.md; apps/host_gui/README.md:58 and assets/ir/README.md:3 cite default_4x12.wav; SUSHI_STANDARDS_MIGRATION.md:36-37,82 cite docs/CLAUDE.md, docs/AGENTS.md, docs/api/mainpage.md; CLAUDE.md:7,19 name 'sushisystemsprogramming'. Lines 36-39 of the migration document also describe root CLAUDE.md as a one-line pointer, which it is not (it is the full 60-line file).

#### D16. [medium] tools/README.md is a verbatim copy of the sushiskills reference and does not describe this repository's tools/

Evidence: D:/Projects/sushidsp/tools/README.md is byte-identical to D:/Projects/sushiskills/tools/README.md (diff -q prints nothing). It opens 'Reference implementations of the four checkers every Sushi Systems repository carries. A repository copies this folder whole; a rule is fixed here first and copied out', which is true of sushiskills and false here. It names neither tools/dsp nor tools/spice, although D:/Projects/sushidsp/docs/README.md describes tools/ as 'Repository checkers and DSP filter design scripts', and it links no sub-README, which is why tools/dsp/README.md and tools/spice/README.md are unreachable from the index.

Recommendation: Write sushidsp's own tools/README.md: one table row per folder (common, documentation, layering, dsp, spice) with a link to tools/dsp/README.md and tools/spice/README.md, and one sentence saying the checkers are copied from sushiskills and are changed there first.

Review: found by review.

#### D17. [medium] The same stale Doxygen path is in the CLI help text, and sd check does not run the source-comment checker the tools README lists

Evidence: D:/Projects/sushidsp/cli/sushidsp/cli.py:132 docstring: 'Generates the API reference (docs/api-site/html) from the headers', against Doxyfile:4 OUTPUT_DIRECTORY = build/docs/api-site. D:/Projects/sushidsp/cli/sushidsp/services/project.py:248-251 runs check_docs_layout.py, check_changelog.py and check_layering.py only; tools/documentation/check_source_comments.py exists and tools/README.md lists it as one of four checkers. README.md and docs/guides/COMMAND_LINE_INTERFACE.md describe sd check as three checks, so the documents match the code, but no document says the fourth checker is run by hand.

Recommendation: Fix the help text together with F11. Decide with the owner whether sd check runs check_source_comments.py; then state the answer once in the CLI guide.

Review: found by review.

#### D18. [medium] The audit's F1 fix puts a shared rule into a local copy

Evidence: The audit recommends having 'check_docs_layout.py's link rule cover module READMEs' in sushidsp. D:/Projects/sushidsp/tools/documentation/check_docs_layout.py is the copied reference checker (tools/README.md: 'a rule is fixed here first and copied out'); its rule_links (line 147) and rule_module_readmes (line 173) are shared across repositories.

Recommendation: SOLID named item: change the link rule in D:/Projects/sushiskills/tools/documentation/check_docs_layout.py and copy it out to every repository, so the siblings stay the same shape. A sushidsp-only edit forks the checker.

Review: found by review.

#### D19. [low] Archive and agent material is filed under names the layout does not allow

Evidence: docs/archive/superpowers/{plans,specs}/, docs/archive/research/, docs/archive/notes/ use dated lower-case names (2026-08-25-asio-backend.md, 2026-08-26-d5b-plugin-readiness-foundations.ledger.md) rather than UPPER_SNAKE_CASE or a kept docs/agent work-folder path. docs/archive/notes/2026-09-22-wave-1-task-7e.md is 13 days old and docs/agent/2026_09_15_SUSHI_STANDARDS_PHASE_1 has a copy of its plan at docs/archive/superpowers/plans/2026-09-15-sushi-standards-phase-1.md (same work in two places). docs/agent/2026_09_20_V0_1_0_RELEASE/REPORT.md is titled 'What jrb32's traced schematic settles' and its status reads 'closed', which is about the Mark IIC+ schematic, not the release the folder names.

Recommendation: Archive is frozen, so do not rename without the owner's decision; record the exception in archive/README.md. Remove the duplicated standards plan from one side and move the jrb32 report's facts into modules/nodes/mesa_mark2c/README.md.

Review: confirmed. Dated lower-case names under docs/archive/superpowers, research, notes; docs/archive/superpowers/plans/2026-09-15-sushi-standards-phase-1.md exists beside docs/agent/2026_09_15_SUSHI_STANDARDS_PHASE_1/PLAN.md; docs/agent/2026_09_20_V0_1_0_RELEASE/REPORT.md is titled 'What jrb32's traced schematic settles'. The recommendation to move the report's facts into the Mesa README is redundant: the report's own line 3 says they are already there. The fix is the folder's name or the report's home.

#### D20. [low] Missing module READMEs beside the code

Evidence: No README.md in D:/Projects/sushidsp/apps/host, apps/host_asio, cli, tests/measure, tests/benchmark, tools/documentation, tools/layering, or any of include/SushiDSP/{core,math,math/dk,circuits,device,io,audio_format}. docs/README.md links include/SushiDSP/math/dk/, circuits/models/ and tests/measure/ as bare directories and states their facts in the index line, which the style guide's own placement rule forbids ('docs/README.md links to it and states no fact of its own').

Recommendation: Add a short README to each (cli and the two hosts first) and reduce the index lines to links.

Review: confirmed. No README.md in any of the 15 listed directories (tests/ itself included). docs/README.md states facts on its index lines for math/dk, circuits/models and tests/measure.

#### D21. [low] Prose that breaks the humanizer register

Evidence: docs/guides/COMMAND_LINE_INTERFACE.md: 28 em dashes in 884 words. docs/CONTRIBUTING.md: 26 in 1094, plus bold-label bullets and 'very welcome'. docs/reference/API_REFERENCE.md: 9 in 204 words. docs/design/SUSHIDSP.md: 123, with line 4 a single 150-word blockquote addressed 'For the implementing agent' that restates coding conventions. docs/reference/KNOWN_ISSUES.md first entry is one 25-line paragraph of nested parentheticals with eleven figures per sentence. CLAUDE.md: 'extremely high, borderline obsessive standard', 'Always be honest, never lie'. docs/README.md index descriptions are noun-triples ('Documented defect tracking, reproduction steps, and planned resolutions').

Recommendation: Rewrite CONTRIBUTING.md and the CLI guide in the root README's register (they need rewriting anyway under F9); break the KNOWN_ISSUES alias-floor entry into a table of figures plus three sentences; leave SUSHIDSP.md body for a separate pass.

Review: confirmed. Em-dash counts reproduce exactly: CONTRIBUTING 26/1094, CLI guide 28/884, API_REFERENCE 9/204, OVERVIEW 11/729, SUSHIDSP 123, README 0/774. CLAUDE.md:42,48 phrases present. KNOWN_ISSUES first entry was not re-read.

### Not checked

- The body of docs/design/SUSHIDSP.md (908 lines), GUI_UX_REDESIGN.md and ROOM_FURNISHINGS.md against the code beyond cited paths and status lines.
- docs/reference/KNOWN_ISSUES.md entries (28) against the tests and figures they cite; only headings and the first entry were read.
- The content of the 12 module READMEs beyond links and line-1 paths (constants, stand-in tables, class names).
- The 52 archived documents' content and their internal links beyond the automated link check (which found none broken there).
- docs/agent SPEC/PLAN/REPORT bodies beyond their first lines.
- docs/reference/spice/*/README.md, tools/dsp/README.md, tools/spice/README.md, tests/golden/README.md, tests/fixtures/*.README.md, apps/host_gui/README.md and .github/ templates were link-checked but not read for accuracy.
- Whether sushicore's register_provision_commands really adds exactly setup, doctor, link and unlink with the flags the guide lists (--toolchain, --no-gpu, --for, --workspace); sushicore source was not opened.
- Whether hub add / hub install-cli accept sushidsp; only that those command names exist in sushihub was seen.
- The real test count: no build or test was run, so 1007 against the 1120 macro count is unresolved.
- Anchors inside links (#section) were not verified, only the file part.
- Backticked bare file names without a directory (e.g. `RoomNode.cpp` in the changelog) were not resolved.
- External URLs (sushisystems.io install scripts, GitHub repo URL).
- Whether sd check (check_docs_layout.py, check_changelog.py) passes; the checkers were not run.
- The 90-day archive rule against git dates per file.

## Layout and hygiene

### Facts

- Root, tracked: README.md, LICENSE (11558 bytes), NOTICE, AGENTS.md (one line, 'See CLAUDE.md.'), CLAUDE.md, CMakeLists.txt, CMakePresets.json, Doxyfile, sushi-module.toml, .gitattributes, .gitignore, .github/ (7 files).
- Root folders, tracked file counts: apps/ 87 (host, host_asio, host_gui), assets/ 3 (hrtf, ir), cli/ 21, cmake/ 3, docs/ 115, include/ 60 (include/SushiDSP/{audio_format,circuits,core,device,io,math}), modules/ 157, src/ 29, tests/ 171, third_party/ 3 (dr_libs), tools/ 14.
- Root, untracked and ignored: build/ (1.5 GB), imgui.ini (267 bytes), .claude/ (settings.local.json), .superpowers/ (16 MB), third_party/asio/ (10 MB, proprietary Steinberg SDK), cli/sushidsp_cli.egg-info/, cli/.pytest_cache/, __pycache__ folders under cli/ and tools/.
- modules/ holds three tiers: nodes (cabinet, capture, dd3, diezel_vh4, jcm800, mesa_mark2c, peavey5150, room, routing, tubescreamer), catalog (node_registry), session (rig_preset). tools/layering/check_layering.py declares K_TIER_ORDER = (nodes, catalog, session).
- Versions agree at 0.1.0: CMakeLists.txt:2 `project(SushiDSP VERSION 0.1.0 LANGUAGES CXX)`, cli/pyproject.toml:7 `version = "0.1.0"`, docs/reference/CHANGELOG.md heading `## v0.1.0 — 2026-10-03`. sushi-module.toml carries no version field. No package.json.
- Git tags: none (`git tag -l` prints nothing), although commit de235ac is `chore(release): v0.1.0` dated 2026-10-03.
- Changelog: `## Unreleased` holds one entry (2026-10-04, cli help panel from sushicore, requires sushicore 0.7.0); 346 entry lines in the file in total.
- CI: one workflow, .github/workflows/ci.yml, triggers on push to main and on pull_request; jobs functional (ubuntu), host-build (ubuntu, SDL2 on), windows. Other .github files: CODE_OF_CONDUCT.md, SECURITY.md, PULL_REQUEST_TEMPLATE.md, three issue templates.
- tools/: common/checker.py (shared rule-table runner, 107 lines), documentation/check_source_comments.py (248), documentation/check_docs_layout.py (211), documentation/check_changelog.py (126), layering/check_layering.py (142), plus dsp/ (bras_reference.py, convert_hrtf.py, design_halfband.py, design_resampler.py, motus_reference.py) and spice/export.py. All four required checkers are present under the required names.
- .gitignore (30 lines): build/, *.obj, *.o, CMakeCache.txt, CMakeFiles/, cli/config.local.toml, cli/*.egg-info/, cli/build/, cli/.pytest_cache/, __pycache__/, docs/api-site/html/, docs/api-site/doxygen-warnings.log, third_party/asio/, /*.wav, /*.png, /imgui.ini.
- .gitattributes marks *.hrtf and *.wav as binary.
- git status: working tree clean, no untracked non-ignored files; main is 5 commits ahead of origin/main. 655 commits on main.
- Remotes: origin = https://github.com/SushiSystems/SushiDSP.git (matches the product name and sushi-module.toml `repo`); a second remote named `local` whose URL is the literal string `local`, with eight remote-tracking branches (main, main2, main3, main4, mainD2, mainD3, mainD4, mainDocs), all dated 2026-08-25.
- Largest tracked files: assets/hrtf/sadie2_ku100_48k.hrtf 1.62 MB; eight docs/reference/spice/*/*_tran.csv at 387-406 KB each; six tests/golden/*_96k.f32 at 384 KB each; src/math/ResamplerKernel.hpp 364 KB.
- No tracked png, jpg, wav, exe, dll, pyc, pdf or zip; the only tracked .json is CMakePresets.json. No cmake discovery JSON or compiler probe header is tracked or present at the root.
- docs/ carries the standard skeleton: README.md, CONTRIBUTING.md, DOCUMENTATION_STYLE_GUIDE.md, getting_started/, architecture/, guides/, reference/ (CHANGELOG, GLOSSARY, KNOWN_ISSUES, API_REFERENCE, spice/), design/ (with REMAINING_WORK.md), agent/, archive/. There is no docs/modules/ folder in the listing.

### Corrections from review

- Remotes: there is no second remote named `local`. `git remote -v` lists only origin and `git config --get remote.local.url` prints nothing. The 'Fetch URL: local' the audit quotes is what `git remote show -n` prints for any unconfigured name. What exists is eight orphan remote-tracking refs under refs/remotes/local/, all fully reachable from main (0 commits ahead each).
- F11 / largest-files fact: src/math/ResamplerKernel.hpp is not presented as hand-written; line 24 marks it `GENERATED by tools/dsp/design_resampler.py - do not edit by hand`.
- tools/ fact list omits three tracked files: tools/README.md, tools/dsp/README.md, tools/spice/README.md (the count of 14 is right, the enumeration names 11).
- ci.yml line citations are shifted: build commands are at 48-53, 72-73, 84-86 (audit: 43-51, 67-70, 80-84); triggers at 28-31 (audit: 30-33); the Python CLI comment at 91-92 (audit: 89-91).
- Recomputed and correct: 317 of 453 PascalCase .hpp/.cpp; 346 changelog entry lines; 655 commits; tracked counts per root folder (apps 87, assets 3, cli 21, cmake 3, docs 115, include 60, modules 157, src 29, tests 171, third_party 3, tools 14, .github 7); versions 0.1.0 in CMakeLists.txt:2, cli/pyproject.toml:7 and the changelog heading; no tags.

### Findings

#### L1. [high] v0.1.0 was released in the changelog and by commit but never tagged

Evidence: `git tag -l` in D:/Projects/sushidsp prints nothing. Commit de235ac `chore(release): v0.1.0` (2026-10-03 13:14:43 +0300) and D:/Projects/sushidsp/docs/reference/CHANGELOG.md `## v0.1.0 — 2026-10-03` both declare the release. versioning-and-release step 6 requires an annotated tag on the release commit.

Recommendation: With the owner's approval, create the annotated tag v0.1.0 on de235ac, or fold the heading back into Unreleased if the release was not meant to be cut yet.

Review: confirmed. `git tag -l` is empty; de235ac is `chore(release): v0.1.0` (2026-10-03 13:14:43 +0300); CHANGELOG.md line 7 is `## v0.1.0 — 2026-10-03`. The release commit is also one of the five unpushed commits.

#### L2. [high] CI does not run the four checkers and bypasses the project CLI

Evidence: D:/Projects/sushidsp/.github/workflows/ci.yml lines 43-51, 67-70 and 80-84 invoke `cmake -B build`, `cmake --build` and `ctest` directly; no step runs tools/documentation/check_*.py or tools/layering/check_layering.py. The file's own header (lines 10-21) admits the exception to the `sd`-only rule. continuous-integration: 'CI calls the project CLI and nothing else' and 'Every push and pull request: the four checkers'.

Recommendation: Add a checker job (standard-library Python, no sushicore needed) and route build and test through `sd` once sushicore can be installed on a runner; until then record the exception in docs/reference/KNOWN_ISSUES.md rather than in a workflow comment.

Review: confirmed. Read ci.yml: cmake/ctest called directly, no checker step. Cited line numbers are off: the commands are at 48-53, 72-73 and 84-86, the exception comment at 10-20. KNOWN_ISSUES.md has no entry for it. The comment also cites `docs/CLAUDE.md`, which does not exist (CLAUDE.md is at the root).

#### L3. [high] Every module keeps its private code in src/, so the layering checker's private-reach rule cannot fire

Evidence: All 12 module trees (for example D:/Projects/sushidsp/modules/nodes/cabinet/, D:/Projects/sushidsp/modules/session/rig_preset/) hold CMakeLists.txt, README.md, include/ and src/; `git ls-files modules` has 62 paths under /src/ and 0 under /source/. D:/Projects/sushidsp/tools/layering/check_layering.py line 26 defines K_CMAKE_SOURCE_REACH as `([A-Za-z0-9_]+)/source\b` and line 128 reports `reaches into {name}/source`. With folders named src the rule matches nothing, so a clean run says nothing about private reaches. The audit listed module contents under not_checked and F6 covers only the root src/.

Recommendation: Rename src to source in each module in the same mechanical change as F6, so the checker stays identical to its siblings instead of gaining a per-repository folder constant.

Review: found by review.

#### L4. [medium] Workflow file is not named after its trigger and no release workflow exists

Evidence: D:/Projects/sushidsp/.github/workflows/ holds only ci.yml. continuous-integration requires one file per trigger, `push.yml` and `release.yml`, and a packaging build on a `vX.Y.Z` tag push. ci.yml line 30-33 triggers on push to main and pull_request only; no tag trigger.

Recommendation: Rename ci.yml to push.yml and add release.yml triggered on `v*.*.*` tags.

Review: confirmed. .github/workflows holds only ci.yml; triggers are at lines 28-31 (not 30-33), push to main and pull_request, no tag trigger.

#### L5. [medium] The Python CLI test job was removed from CI to get a runnable workflow

Evidence: D:/Projects/sushidsp/.github/workflows/ci.yml lines 89-91: 'The Python CLI job. `cli/` needs sushicore 0.6.0, which is not on PyPI, so a public clone cannot install it; its tests run locally.' Commit 6a5285b `build(ci): drop the Python CLI job a public clone cannot run`. Seven test files under D:/Projects/sushidsp/cli/tests/ have no CI coverage. The comment also names sushicore 0.6.0 while the Unreleased changelog entry says the CLI now requires 0.7.0.

Recommendation: Restore the job once sushicore is published or installable from its git URL; correct the stale version in the comment.

Review: confirmed. Comment is at ci.yml lines 91-92 (not 89-91); commit 6a5285b exists; cli/tests has 7 test_*.py plus conftest.py; pyproject.toml requires sushicore>=0.7.0 while the comment says 0.6.0.

#### L6. [medium] Repository mixes the one-module and many-module layouts

Evidence: D:/Projects/sushidsp/include/SushiDSP/ (60 tracked files) and D:/Projects/sushidsp/src/ (29 files: audio_format, circuits, core, device, io, math) sit at the root beside D:/Projects/sushidsp/modules/{nodes,catalog,session}/ (157 files). repository-layout: with more than one module, each module is a tree under modules/<tier>/<module>/; root include/ and source/ belong to the one-module case only. check_layering.py K_TIER_ORDER (line 22) lists only nodes, catalog, session, so the root core is outside the layering check.

Recommendation: Owner decision on module boundary: move the core under modules/<tier>/ (for example a `core` tier below nodes) and add that tier to K_TIER_ORDER.

Review: confirmed. include/ 60 and src/ 29 tracked files beside modules/ 157; K_TIER_ORDER at check_layering.py:22 is (nodes, catalog, session). The recommendation leaves the boundary to the owner, which is right; a single `core` module holding six areas (audio_format, circuits, core, device, io, math) would not be one responsibility, so the move should split per area.

#### L7. [medium] Top-level folder names depart from the shared tree

Evidence: D:/Projects/sushidsp/src/ where the layout names `source/`; D:/Projects/sushidsp/apps/ where it names `applications/`; D:/Projects/sushidsp/assets/ (hrtf/, ir/) is a top-level folder not in the root table ('New top-level folder: it is not allowed; pick a home from the table').

Recommendation: Rename src to source and apps to applications in one mechanical change; decide a home for assets/ (the application that loads them, or the room module) with the owner.

Review: confirmed. Root listing shows src/, apps/, assets/; none is in the layout's root table.

#### L8. [medium] Doxyfile is committed at the root, which the layout forbids

Evidence: D:/Projects/sushidsp/Doxyfile (1237 bytes, tracked). repository-layout lists `Doxyfile` under 'Forbidden in the root'. D:/Projects/sushidsp/.gitignore lines 12-13 explain that `sd doxygen` reads a repo-root Doxyfile and fails without it.

Recommendation: Move the Doxyfile under docs/ (docs/api-site/ is already the ignore target) and point `sd doxygen` at it; remove the justification comment from .gitignore.

Review: confirmed. Doxyfile is tracked at the root, 1237 bytes; .gitignore lines 12-13 carry the justification.

#### L9. [medium] .clang-format and .editorconfig are missing

Evidence: `ls .clang-format .editorconfig` in D:/Projects/sushidsp: 'cannot access'. Both are listed in the required root set of repository-layout.

Recommendation: Copy the two files from the reference repository so every sibling formats the same way.

Review: confirmed. `ls -la` of the root shows neither .clang-format nor .editorconfig; `git ls-files` lists neither.

#### L10. [medium] tests/ does not follow the unit / integration / regression / common shape

Evidence: D:/Projects/sushidsp/tests/ contains benchmark, circuits, common, fixtures, functional, golden, measure (171 tracked files). repository-layout prescribes tests/{unit,integration,regression,common} with fixtures at tests/<kind>/fixtures/, and per-module tests under modules/<tier>/<module>/tests/. Golden files sit at tests/golden/ and fixtures at tests/fixtures/, outside any kind folder.

Recommendation: Regroup into the four kinds, moving golden and fixtures under the kind that reads them; benchmark and measure need an owner decision on their home.

Review: confirmed. tests/ holds benchmark (3), circuits (3), common (3), fixtures (7), functional (131), golden (15), measure (8) plus CMakeLists.txt = 171; no modules/*/*/tests folder exists.

#### L11. [medium] Source files are PascalCase, not snake_case

Evidence: 317 of 453 tracked .hpp/.cpp files start with a capital letter, for example D:/Projects/sushidsp/src/math/ResamplerKernel.hpp, Biquad.cpp, PartitionedConvolution.cpp, StreamingConvolution.cpp. cmake modules likewise: D:/Projects/sushidsp/cmake/CompilerWarnings.cmake, Sanitizers.cmake, SushiDSPCompilerFlags.cmake. repository-layout: 'Folders and files: snake_case ... physics_world.hpp holds PhysicsWorld'.

Recommendation: Plan a mechanical rename wave (git mv plus include rewrites); it is a public include path change, so it needs approval and a MINOR bump before v1.0.0.

Review: confirmed. Recomputed: 453 tracked .hpp/.cpp, 317 with a capital first letter. cmake/ holds CompilerWarnings.cmake, Sanitizers.cmake, SushiDSPCompilerFlags.cmake.

#### L12. [medium] Tracked binary and bulk data with no LFS or size policy

Evidence: D:/Projects/sushidsp/assets/hrtf/sadie2_ku100_48k.hrtf 1619940 bytes; six D:/Projects/sushidsp/tests/golden/*_96k.f32 at 384000 bytes each; eight D:/Projects/sushidsp/docs/reference/spice/*/*_tran.csv at 387-406 KB each (about 3.1 MB of simulator output inside the manual). .gitattributes marks only *.hrtf and *.wav binary; *.f32 is not listed.

Recommendation: Add `*.f32 binary` to .gitattributes; move the SPICE transient CSVs out of docs/reference/ to a test fixture folder, since they are reference data and not prose.

Review: confirmed. Sizes recomputed: hrtf 1619940; six *_96k.f32 at 384000; eight *_tran.csv 387391-405551, sum about 3.12 MB. .gitattributes has only *.hrtf and *.wav. The audit omits the 48k golden files at 192000 each, which does not change the conclusion.

#### L13. [medium] Five commits are unpushed and a dead `local` remote holds eight stale branches

Evidence: `git status -sb`: `## main...origin/main [ahead 5]`. `git remote show -n local`: 'Fetch URL: local'. Remote-tracking refs local/main, main2, main3, main4, mainD2, mainD3, mainD4, mainDocs, all dated 2026-08-25.

Recommendation: Push main when the owner says so. Confirm with the owner that the eight local/* refs hold nothing unmerged, then remove the remote.

Review: confirmed. `[ahead 5]` and the eight local/* refs dated 2026-08-25 are real. One part is wrong: no remote named `local` is configured (`git remote -v` lists only origin, `git config --get remote.local.url` is empty), so 'remove the remote' cannot be done as written; the refs are orphans. All eight are reachable from main (`git rev-list --count main..local/<ref>` = 0 for each), which answers the audit's own open question.

#### L14. [medium] No module carries its own tests/ folder; all module tests sit in the root tests/

Evidence: `ls -d modules/*/*/tests` in D:/Projects/sushidsp finds nothing, while D:/Projects/sushidsp/tests/functional/ holds 131 tracked files. repository-layout: the root tests/ holds 'tests that cross module boundaries only' and each module tree has tests/. F9 mentions the per-module rule in its evidence but its recommendation only regroups the root folder into four kinds.

Recommendation: When F9 is planned, sort tests/functional by the module each file exercises and move single-module tests into modules/<tier>/<module>/tests/; only cross-module tests stay at the root.

Review: found by review.

#### L15. [medium] Root CMakeLists.txt defines targets and fetches dependencies

Evidence: D:/Projects/sushidsp/CMakeLists.txt (7819 bytes): `add_library(sushi_dsp STATIC` with a source list at line 16, target_include_directories at 48-53, FetchContent for SDL2 at 74-83, sushi_dsp_sdl2 at 85-93, sushi_dsp_asio at 113-128, FetchContent json at 138-145, glfw and imgui at 153-168, `add_library(imgui STATIC` at 170. repository-layout: the root file holds 'project(), options and add_subdirectory; no logic', and cmake/ holds every CMake function. The audit listed this under not_checked.

Recommendation: Move each target definition to the CMakeLists.txt of the folder that owns its sources and each FetchContent block to one file under cmake/ per dependency; the root keeps project(), options and add_subdirectory. This follows from F5, since the sushi_dsp target is the root core.

Review: found by review.

#### L16. [medium] No generated version header; the CMake version reaches no C++ code

Evidence: `git ls-files | grep -i version` in D:/Projects/sushidsp returns nothing, and `grep configure_file|PROJECT_VERSION CMakeLists.txt cmake/` returns nothing. versioning-and-release: for C++ the source is project(VERSION) and 'version.hpp is generated from it'. F18 covers the pyproject and manifest copies but not this.

Recommendation: Add a version.hpp.in configured from PROJECT_VERSION under the core's include tree; owner approval needed since it adds a public header.

Review: found by review.

#### L17. [medium] CLAUDE.md names a skill that does not exist, and ci.yml cites a file that does not exist

Evidence: D:/Projects/sushidsp/CLAUDE.md lines 7 and 19 name `sushisystemsprogramming`; the installed skill set has `sushiskills` and no skill of that name. repository-layout: CLAUDE.md 'names the skills the repository follows'. D:/Projects/sushidsp/.github/workflows/ci.yml line 13 cites `docs/CLAUDE.md`, which is absent (CLAUDE.md is at the root).

Recommendation: Replace the name with the current skill names in CLAUDE.md and fix the path in the workflow comment when F2 rewrites that header.

Review: found by review.

#### L18. [medium] The changelog cites docs/archive/changelog/, which is absent

Evidence: D:/Projects/sushidsp/docs/reference/CHANGELOG.md line 9 cites `docs/archive/changelog/`; D:/Projects/sushidsp/docs/archive/ holds only README.md, notes/, research/ and superpowers/. The same entry records that untagged v0.1.0, v0.2.0 and v0.3.0 headings existed before, so F1 is the second time a release heading was written without a tag.

Recommendation: Decide with the owner whether the folded sections were meant to be archived; either create the folder with them or correct the cited path. Treat F1 as a repeat and tag at release time from now on.

Review: found by review.

#### L19. [low] 16 MB of earlier-agent scratch sits in the repository root

Evidence: D:/Projects/sushidsp/.superpowers/sdd/ holds nine run folders (2026-08-25-imgui-rig-gui ... amp-gain-staging) with files such as final-fix-report.md, progress.md, review-327769a..41989ab.diff. It is ignored, but `git check-ignore -v` names no rule in the repository's .gitignore for it or for .claude/, so the ignore comes from outside the repository.

Recommendation: Ask the owner whether anything there is still wanted; anything worth keeping belongs in docs/agent/reports/ or docs/archive/, the rest is deleted. Add `.superpowers/` and `.claude/settings.local.json` to .gitignore so a fresh clone on another machine behaves the same.

Review: confirmed. .superpowers/sdd has nine run folders, 16M; `git check-ignore -v` matches only imgui.ini and third_party/asio, so .claude and .superpowers are ignored from outside the repository.

#### L20. [low] imgui.ini and root screenshots are ignored rather than prevented

Evidence: D:/Projects/sushidsp/imgui.ini (267 bytes, 2026-09-25) exists at the root; D:/Projects/sushidsp/.gitignore lines 28-30 ignore `/*.png` and `/imgui.ini`, lines 25 `/*.wav`. host_gui writes its layout cache into the working directory.

Recommendation: Set ImGui's IniFilename to a per-user configuration path in apps/host_gui so the file never lands in the checkout; keep the ignore rules as a backstop.

Review: confirmed. imgui.ini 267 bytes at the root; .gitignore lines 25, 29, 30; `grep -rn IniFilename apps` returns nothing, so ImGui's default working-directory path is in use.

#### L21. [low] .gitignore carries multi-line rationale and misses common entries

Evidence: D:/Projects/sushidsp/.gitignore lines 12-13, 17-19, 22-24, 27-28 are explanatory paragraphs. No rule for build-asan/, .venv/, venv/, *.egg-info/ outside cli/, .vscode/, .idea/, compile_commands.json or CMakeUserPresets.json.

Recommendation: Reduce the comments to one line each, move the reasons to docs/CONTRIBUTING.md, and add the missing patterns shared by sibling repositories.

Review: confirmed. .gitignore comment runs at 12-13, 17-19, 22-24, 27-28; no rule for build-asan/, venv, .vscode, .idea, CMakeUserPresets.json. 'Shared by sibling repositories' was not shown by the audit and I did not check siblings.

#### L22. [low] tools/ holds generators and exporters beside the checkers

Evidence: D:/Projects/sushidsp/tools/dsp/ (bras_reference.py, convert_hrtf.py, design_halfband.py, design_resampler.py, motus_reference.py) and D:/Projects/sushidsp/tools/spice/export.py are not checkers; D:/Projects/sushidsp/tools/README.md describes only the four checkers and common/checker.py and does not list dsp/ or spice/.

Recommendation: List dsp/ and spice/ in tools/README.md with one line each, or expose them through `sd` so that everything runnable is reachable from one place.

Review: confirmed. tools/README.md lists only the four checkers and common/checker.py. The audit omits that tools/dsp/README.md (37 lines) and tools/spice/README.md (55 lines) exist, so the scripts are documented, just not linked from the index.

#### L23. [low] sushi-module.toml declares no version

Evidence: D:/Projects/sushidsp/sushi-module.toml holds name, alias, distribution and repo only. The version exists in CMakeLists.txt:2 and cli/pyproject.toml:7, two hand-kept copies with no check that they match.

Recommendation: Name CMakeLists.txt the single source and have the CLI package read or verify it; decide across the stack whether the manifest carries a version.

Review: confirmed. sushi-module.toml has name, alias, distribution, repo only; CMakeLists.txt:2 and cli/pyproject.toml:7 both say 0.1.0 with no check between them.

#### L24. [low] Extra root files outside the layout's root set

Evidence: D:/Projects/sushidsp/NOTICE, D:/Projects/sushidsp/CMakePresets.json and D:/Projects/sushidsp/sushi-module.toml are tracked at the root; none is in the repository-layout root list. CMakePresets.json and sushi-module.toml are tool-mandated at the root; NOTICE accompanies the current Apache-style LICENSE (11558 bytes).

Recommendation: Add the two tool-mandated files to the layout skill's root list so the rule matches practice; revisit NOTICE when the licence changes.

Review: confirmed. NOTICE (1822 bytes), CMakePresets.json and sushi-module.toml are tracked at the root and absent from the skill's root list. Low severity is right; the recommendation edits the skill, which is outside this repository.

### Refuted

- A 364 KB generated coefficient table is tracked as hand-written source: Size 363932 is right, but the claim 'as hand-written source' is wrong: src/math/ResamplerKernel.hpp line 24 reads `// GENERATED by tools/dsp/design_resampler.py - do not edit by hand.` followed by tool versions and measured figures. The header mark the audit asks for already exists; what is left (no linguist-generated attribute, a six-line `//` run that the comment rules forbid) is low severity.

### Not checked

- Contents of each modules/<tier>/<module>/ tree (README.md, CMakeLists.txt, include/, source/, tests/ present or not) were not opened; only the module names were listed.
- Whether CMakeLists.txt (7819 bytes) holds logic beyond project(), options and add_subdirectory was not read.
- The checkers were not run (read-only task, no project tooling), so their current finding counts are unknown; their conformance to the contract was checked only for check_layering.py's usage line and K_ constants and the tools/README.md description.
- LICENSE and NOTICE text and the source file licence headers (another dimension).
- Contents of docs/ subfolders beyond a listing of docs/agent, docs/design, docs/archive and docs/reference; absence of docs/modules/ was seen in the listing but not confirmed against the docs checker.
- Contents of build/ (skipped by instruction), of third_party/asio/, and of .claude/settings.local.json.
- Whether the eight local/* remote-tracking branches contain commits not reachable from main.
- Whether origin on GitHub has tags or releases that the local clone lacks (no fetch was run).
- cli/config.toml and cli/sushistack.deps.toml contents; cli/pyproject.toml beyond its version line.
- File name casing outside .hpp/.cpp (shaders, Python, tests data), and the one-primary-symbol-per-header rule.

## Code shape

### Facts

- 681 tracked files at b3a7f00: 264 .cpp, 189 .hpp, 1 .h, 28 .py, 116 .md; C++ and Python only, no TypeScript, no GLSL. About 95.9k lines of C++/Python outside third_party.
- Core library `sushi_dsp` (include/SushiDSP + src): 89 C++ files, about 15.9k lines (4.4k of them the generated table src/math/ResamplerKernel.hpp). Areas: audio_format, circuits, core, device, io, math.
- Measured include direction inside the core library: circuits -> math; io -> math; device -> core, audio_format; core -> io (one edge, ICabinetHost.hpp:26 includes io/Ir.hpp). math includes only math. No cycle found.
- modules/: 133 C++ files, about 22.4k lines. nodes/room 73 tracked files (about 11.2k lines), capture 22, dd3 13, routing 8, jcm800/peavey5150/diezel_vh4/mesa_mark2c 6 each, tubescreamer 5, cabinet 4; catalog/node_registry 4; session/rig_preset 4.
- Module include direction: every nodes/* module includes only core, math, circuits, io and itself; no node includes another node. catalog/node_registry includes all nine registered node modules (capture is not registered); session/rig_preset includes core and node_registry. No cycle found.
- apps/: 83 C++ files, about 14.6k lines. host (4 files), host_asio (2), host_gui (81 tracked incl. README/CMake; ui/ subfolder holds the ImGui views).
- cli/: 17 Python files, 1,337 lines (Typer CLI `sd`, services/project.py 322 lines). tools/: 11 Python files, 3,196 lines (checkers plus DSP/SPICE design scripts).
- tests/: 148 C++ files, about 38.5k lines; one GoogleTest binary `sushidsp_tests`. tests/functional/unit 125 files, tests/functional/regression 6 files, tests/golden 12 .f32 files, tests/benchmark, tests/measure (8 headers), tests/circuits, tests/common. cli/tests holds 7 pytest files.
- Largest source files 1-10: src/math/ResamplerKernel.hpp 4399 (generated); tests/functional/unit/test_room_node.cpp 2278; test_rig_controller.cpp 1986; test_rt_safety.cpp 1740; tools/dsp/convert_hrtf.py 1119; test_ts_clip_stage.cpp 1084; apps/host_gui/SlotGraph.cpp 975; test_rig_graph.cpp 925; tests/benchmark/main.cpp 887; modules/nodes/mesa_mark2c/src/MesaMark2c.cpp 866.
- Largest source files 11-20: include/SushiDSP/math/dk/DkCircuit.hpp 837; apps/host_gui/RigPersistence.cpp 785; test_jcm800.cpp 784; apps/host_gui/main.cpp 776; modules/nodes/peavey5150/src/Peavey5150.cpp 766; test_mesa_mark2c.cpp 726; apps/host_gui/ui/RoomPanel.cpp 697; test_modal_solver.cpp 692; apps/host_gui/SessionTimeline.cpp 690; test_measure.cpp 666.
- Test layout: no module carries a tests/ folder beside it (0 of 12 under modules/); every unit test sits in root tests/functional/unit, including tests of apps/host_gui classes, whose .cpp files tests/CMakeLists.txt:125-137 compiles into the test binary. There is no tests/integration folder.
- Headers no test file includes: apps/host_gui Mp3Decoder and 15 ui/ view headers (ArrangementView, DeviceSettings, Divider, FileBrowser, GearBrowser, Geometry, MeterBar, RigChainView, RoomPanel, StagePanel, TextField, Theme, TopBar, TransportBar, Widgets); device/AsioAudioDevice, device/Sdl2AudioDevice; core ICabinetHost, ParameterChange, ParameterTable, PortLayout; circuits SoftTransition, GridConductionLaw, PlateSwing, PowerTubePresets; math/dk/LinearPort; the three amp ToneStack headers (DiezelVh4, MesaMark2c, Peavey5150); room BinauralStage, DelayRing, DirectPath, FadeWindow, FirFilter, FrameDelay, GeometricPath, ModalPath. Several of these are reached indirectly through a parent class.
- No area has zero tests except: apps/host and apps/host_asio main programs, the two device backends, and all of tools/ (11 Python files, no test).
- No logger exists in the repository: zero `_LOG_` macros, zero log category headers. 27 stdout/stderr calls live in the three apps' main.cpp files only.
- No `.clang-format` is tracked at the repository root.
- All 672 distinct C++ constants use `kCamelCase`; zero use `K_UPPER_SNAKE`. All C++ files and classes use PascalCase file names (e.g. RigGraph.hpp), not snake_case.
- Module verdicts. core library math/: needs targeted fixes. circuits/: needs targeted fixes. core/: needs targeted fixes. io/, audio_format/, device/: needs targeted fixes (error model, headers).
- Module verdicts. nodes/room: needs targeted fixes (comment shape is the best in the tree; error model and public surface width remain). nodes/capture: needs targeted fixes. nodes/dd3: sound apart from repo-wide naming. nodes/routing, cabinet, tubescreamer: needs targeted fixes (file headers).
- Module verdicts. nodes/jcm800, peavey5150, diezel_vh4, mesa_mark2c: needs targeted fixes (shared amp brick, stacked comments). catalog/node_registry: needs targeted fixes. session/rig_preset: needs targeted fixes (nlohmann in public header).
- Module verdicts. apps/host_gui: needs restructuring (RigController/SlotGraph/main). apps/host, apps/host_asio: needs targeted fixes. cli/: needs targeted fixes. tools/: needs targeted fixes. tests/: needs restructuring of layout only (content is broad).

### Corrections from review

- Stdout/stderr calls in the three apps' main.cpp files: the audit says 27; a recount gives 23 (apps/host/main.cpp 4, apps/host_asio/main.cpp 15, apps/host_gui/main.cpp 4). The 'main.cpp only' part holds once snprintf is excluded.
- Distinct kCamelCase constants: the audit says 672; a recount over tracked C++ outside third_party gives 785 declared with const/constexpr (846 distinct k-identifiers in all). K_ count of 0 is correct, so the conclusion stands.
- Unsized enums (F11): the audit lists 7; there are 8. modules/nodes/dd3/include/SushiDSP/dd3/Dd3Converter.hpp:38 (enum class Dd3Mode) is missing, which also weakens the 'dd3: sound apart from repo-wide naming' verdict slightly.
- '///' blocks longer than eight lines (F2): the audit says 53; my scan of the same file set gives 66.
- Files referencing plan tasks or rulings (F3): the audit says 7; a search for 'Task N.N' or 'ruling' finds 14 files, among them include/SushiDSP/math/ResamplerDesign.hpp, include/SushiDSP/math/dk/DkCircuit.hpp, modules/nodes/dd3/include/SushiDSP/dd3/Dd3Delay.hpp and modules/nodes/tubescreamer/src/TubeScreamer.cpp.
- F5 cites RoomNode.hpp as using 'std::string& error_out'; the parameter there is 'reason_out' (RoomNode.hpp:114, :148), and set_hrtf at :116 returns a bare bool.
- F6 says SlotGraph.hpp:95-311 declares about 45 public methods; I count roughly 40 in that span (public section is :70-320).
- Recomputed and correct: 681 tracked files at b3a7f00 (264 .cpp, 189 .hpp, 1 .h, 28 .py, 116 .md); 95,916 C++/Python lines outside third_party; 256 files without @file and 260 without @author; 2,241 stacked pairs in 152 files; the twenty largest files and their line counts; module file counts (core 89, modules 133, apps 83, tests 148).

### Findings

#### C1. [high] 256 of 453 C++ files have no @file block; 260 have no @author

Evidence: Missing @file by area: include/SushiDSP 50 of 60 (e.g. include/SushiDSP/core/IAudioNode.hpp:23 goes from the license block straight to `#pragma once`), src 25 of 29, apps/host_gui 50 of 79, apps/host 3, apps/host_asio 1, modules/nodes/capture 18 of 20, routing 6 of 6, jcm800 4 of 4, peavey5150 4 of 4, cabinet 2, tubescreamer 2 of 3, node_registry 2, rig_preset 2, tests/functional 75 of 131, tests/measure 8 of 8. Complete areas: modules/nodes/room (71), dd3 (11), diezel_vh4 (4), mesa_mark2c (4).

Recommendation: Add the three-tag file block to every file in the listed areas, using room and dd3 as the template; the module-by-module wave should start with include/SushiDSP and src, where coverage is lowest.

Review: confirmed. Recomputed over 453 non-third_party C++ files: 256 lack @file, 260 lack @author; per-area numbers match (include 50/60, src 25/29, host_gui 50/79, capture 18/20, tests/functional 75/131, tests/measure 8/8). The evidence list omits four files it counted: tests/benchmark 2, tests/common 1, tests/golden/GoldenFile.hpp 1.

#### C2. [high] Stacked // paragraphs and design rationale in source bodies

Evidence: 2,241 consecutive-`//` line pairs across 152 files. Worst: tests/functional/unit/test_room_node.cpp (166), test_ts_clip_stage.cpp (115), test_diezel_vh4.cpp (111), modules/nodes/peavey5150/src/Peavey5150.cpp (90), modules/nodes/jcm800/src/Jcm800.cpp (72), modules/nodes/diezel_vh4/src/DiezelVh4.cpp (47). Example: Jcm800.cpp:200-211 is a 12-line `//` block explaining why the taper exponent is 1.5, with measurements. Also 53 `///` blocks longer than eight lines, e.g. modules/session/rig_preset/include/SushiDSP/rig_preset/RigPreset.hpp:50-60.

Recommendation: Move the rationale into the module README or a docs/design document and leave a one-line invariant citing the path. The four amp modules and the test files carry most of the volume.

Review: confirmed. Recomputed 2,241 stacked pairs in 152 files; top files match (test_room_node 166, test_ts_clip_stage 115, test_diezel_vh4 111, Peavey5150.cpp 90, Jcm800.cpp 72). Jcm800.cpp:201-213 is the cited rationale block. The '53 /// blocks over eight lines' figure came out as 66 with my scan; conclusion unchanged.

#### C3. [high] The repository has no logger, so handled failures are recorded nowhere

Evidence: Zero `_LOG_` macros and zero `*_log_category.hpp` files under include, src, modules, apps. Failures that are caught and neither logged nor returned with a reason: apps/host_gui/TopologyApplier.cpp:229-232 (`catch (...)` with a comment-only body after rig.publish()); apps/host_gui/SlotGraph.cpp:474-477 (IR load failure only clears cabinet_path); apps/host_gui/ui/DirectoryListing.cpp:117-120 (filesystem_error returns an empty listing); modules/nodes/capture/src/TrackRecorder.cpp:82-86, :104-111, :246-250 (`catch (...)` returns false or sets failed_, the cause is dropped); modules/nodes/room/src/RoomNode.cpp:384-387 (decoder prepare failure returns false with no reason).

Recommendation: Decide with the owner whether SushiDSP gets its own logger in the core library or takes one from a lower Sushi repository; then give each module one category and log at the listed catch sites. This is an external-dependency and module-boundary decision, so it needs the owner.

Review: confirmed. No _LOG_ macro or log category anywhere. Read all cited sites: TopologyApplier.cpp:229-232 swallows with a comment, SlotGraph.cpp:474-477, DirectoryListing.cpp:117-120, TrackRecorder.cpp:82/104/246, RoomNode.cpp:384-387 all drop the cause. The other five catch(...) sites (TopologyApplier.cpp:80, SlotGraph.cpp:964, RigChainView.cpp:334, Mp3Decoder.cpp:117, AsioAudioDevice.cpp:337) rethrow and are correctly left out.

#### C4. [high] Three error mechanisms coexist; exceptions cross public module surfaces

Evidence: Exceptions on public surfaces: src/io/WavFile.cpp:70,78,82,102 (std::runtime_error for unreadable or malformed files), src/core/RigGraph.cpp (7 throws), modules/nodes/room/src/HrtfSet.cpp (10), modules/session/rig_preset/src/RigPreset.cpp (10; RigPreset.hpp:70-74 also documents that nlohmann::json::parse_error escapes). `bool` plus `std::string& error_out`: apps/host_gui/RigController.hpp (12 signatures), SessionTimeline.hpp (14), modules/nodes/room/include/SushiDSP/room/HrtfSetLoader.hpp, RoomNode.hpp. Bare `bool`: TrackRecorder start path (TrackRecorder.cpp:86, :111). No `enum class ...Status` exists for any of these.

Recommendation: Pick one model per the C++ style skill (status enum, `[[nodiscard]]`) and apply it first at io/ (WavFile, IrLoader, WavWriter), rig_preset and HrtfSet, where the failure cause is outside the program. Public interface shape, so the owner decides before the wave starts.

Review: confirmed. Throw counts match (RigGraph.cpp 7, HrtfSet.cpp 10, RigPreset.cpp 10; WavFile.cpp has 8, the audit cites 4 lines). error_out counts match (RigController.hpp 12, SessionTimeline.hpp 14). No 'enum class ...Status' exists. One slip: RoomNode.hpp uses 'std::string& reason_out' (:114, :148), and set_hrtf at :116 is a bare bool.

#### C5. [high] RigController and SlotGraph each hold several responsibilities, and RigController re-declares SlotGraph's surface

Evidence: apps/host_gui/SlotGraph.hpp:95-311 declares about 45 public methods covering graph editing, bypass, parameters, cabinet IR loading (:185), room scenes (:224-240), HRTF path (:247-250), IR folder (:303-306), input source (:311) and snapshot/restore (:261-270). apps/host_gui/RigController.hpp:77-279 repeats the same signatures one for one, then adds the session surface (:291-471); RigController.cpp holds 63 one-line forwards to `graph_` or `session_`. apps/host_gui/main.cpp:221-776 is a single `main()` of about 550 lines that owns window setup, device setup, default assets and the frame loop.

Recommendation: Restructure: let callers take SlotGraph and SessionTimeline directly (or narrow role interfaces) instead of a forwarding facade; split cabinet/room/HRTF asset handling out of SlotGraph; split main() into an application object with setup, frame and shutdown bricks. Module boundary decision for the owner.

Review: confirmed. SlotGraph.hpp public section runs :70-320 with about 40 methods (I count 39 to 42, the audit says about 45); RigController.hpp public runs :55-502; RigController.cpp has 63 one-line forwards; main() starts at main.cpp:221 and runs to the end of a 776-line file. SOLID check: dropping the facade and splitting asset handling is brick-shaped, but 'or narrow role interfaces' leaves the shape open; correctly routed to the owner.

#### C6. [medium] Struct fields, enumerators and many members carry no @brief; non-standard tags in use

Evidence: include/SushiDSP/core/IAudioNode.hpp:34-38 (PrepareInfo fields) and :43-47 (ProcessContext fields); include/SushiDSP/core/NodeDescriptor.hpp:34-41 (ParamKind enumerators), :45-53 (ParameterDescriptor fields), :57-64 (NodeDescriptor fields); include/SushiDSP/core/IAudioDevice.hpp:33-38. `@throws` is used as a tag at RigPreset.hpp:57 and :70 and include/SushiDSP/device/AsioAudioDevice.hpp:69. Plan and task history appears in source: src/math/ResamplerKernel.hpp:28-29 ('grown from the planned 32 ... DR1+DR2 plan Task 1.1 ruling'); 7 files reference plan tasks or rulings.

Recommendation: Give every field and enumerator a `/**< */` sentence; replace `@throws` paragraphs with the status model from F5; have tools/dsp/design_resampler.py stop emitting plan history into the generated header.

Review: confirmed. IAudioNode.hpp:34-38 and :43-47, IAudioDevice.hpp:33-38 fields carry no comment; ResamplerKernel.hpp:28-29 carries the plan-task text. A broader search finds 14 files with Task/ruling references, not 7, so the count is a floor.

#### C7. [medium] Adding a product still edits the GUI, contrary to docs/architecture/OVERVIEW.md

Evidence: apps/host_gui/ui/PanelLibrary.cpp:122-149 is a table keyed by eight hard-coded descriptor ids with per-product control layouts at :34-112; apps/host_gui/ui/GearFilter.cpp:39 tests `id == "sushidsp.splitter" || id == "sushidsp.mixer"`; apps/host_gui/TopologyApplier.cpp:38 and :41 hard-code the same two ids. modules/catalog/node_registry/src/NodeRegistry.cpp:44-56 is the central list every product must be added to, with the category as a free string. OVERVIEW.md states that adding a product means 'adding one entry there and touching nothing in apps/'.

Recommendation: Let each product module own its panel layout and role (routing vs gear) as data on or beside its NodeDescriptor, so PanelLibrary and GearFilter read it rather than name ids. Public interface shape; ask the owner.

Review: confirmed. PanelLibrary.cpp:122-149 keys eight ids, GearFilter.cpp:39 and TopologyApplier.cpp:38/:41 hard-code splitter and mixer, NodeRegistry.cpp:44-56 is the central list, OVERVIEW.md:66 makes the claim. Note room and cabinet and dd3 have no PanelLibrary entry, and RoomPanel/StagePanel are per-product views, which strengthens the finding.

#### C8. [medium] Descriptor and parameter lookup by id is written five times

Evidence: modules/catalog/node_registry/src/NodeRegistry.cpp:61-69 (make_node loop); modules/session/rig_preset/src/RigPreset.cpp:40-50 (descriptor_for_type); apps/host_gui/SlotGraph.cpp:56-75 (find_parameter_index); apps/host_gui/ui/RoomPanel.cpp:159-170 (room_parameter); apps/host_gui/ui/StagePanel.cpp:61-72 (parameter_for). All are the same strcmp scan over node_registry() or NodeDescriptor::parameters.

Recommendation: Add `find_descriptor(id)` to node_registry and a parameter lookup beside NodeDescriptor in core, then delete the four copies.

Review: confirmed. All five cited sites are the same strcmp scan. It is an undercount: SlotGraph.cpp:82 and :101, RigPersistence.cpp:747, RigChainView.cpp:140 and StagePanel.cpp:50 scan node_registry() by id as well. The fix (one find_descriptor plus one parameter lookup) is brick-shaped.

#### C9. [medium] The four amplifier modules duplicate the same private helpers and constants

Evidence: `presence_gain_db` with an identical body: modules/nodes/jcm800/src/Jcm800.cpp:186, peavey5150/src/Peavey5150.cpp:358, diezel_vh4/src/DiezelVh4.cpp:259. `miller_corner_hz`: Jcm800.cpp:195, Peavey5150.cpp:350, DiezelVh4.cpp:230, mesa_mark2c/src/MesaMark2c.cpp:300. `kCoefficientEpsilon = 1e-4f`, `kMinInputTrimDb = -24.0f`, `kMaxOutputDb = 12.0f` repeated at Jcm800.cpp:177-184, Peavey5150.cpp:328-335, DiezelVh4.cpp:217-224, MesaMark2c.cpp:286-293 (epsilon again at routing/src/PanNode.cpp:38). `kOversamplingFactor = 8` in all four amp headers (Jcm800.hpp:163, Peavey5150.hpp:176, DiezelVh4.hpp:176, MesaMark2c.hpp:201). The input_trim/output ParameterTable rows and the descriptor/make boilerplate are also identical (Jcm800.cpp:516-556 vs Peavey5150.cpp:724-764).

Recommendation: Move the shared presence law, trim/output ranges and dirty-epsilon into one brick under include/SushiDSP/circuits (PowerAmpFeedback and MillerInputCapacitance already live there). The later wave should diff the four process() bodies for a shared power-amp chain as well.

Review: confirmed. presence_gain_db bodies are identical at Jcm800.cpp:186, Peavey5150.cpp:358, DiezelVh4.cpp:259; the three constants and kOversamplingFactor = 8 repeat at the cited lines. miller_corner_hz is not one helper: Jcm800 takes one argument, the other three take two with different second parameters, so it needs a parametrised brick, not a move.

#### C10. [medium] nlohmann::json is part of rig_preset's public interface

Evidence: modules/session/rig_preset/include/SushiDSP/rig_preset/RigPreset.hpp:29 includes `<nlohmann/json.hpp>`; :41 and :48 expose `nlohmann::json&` in the `AnnotateNodeFn` and `OnNodeLoadedFn` callback types. The comment at :36-40 notes that the caller may overwrite already-written fields through that handle. The root CMakeLists.txt fetches nlohmann unconditionally because the tests need it through this header.

Recommendation: Replace the JSON handle with a module-owned type (for example a string-keyed extra-fields map per node) so the dependency stays inside RigPreset.cpp.

Review: confirmed. RigPreset.hpp:29 includes nlohmann/json.hpp and :41/:48 expose nlohmann::json&. Extra evidence the audit did not cite: modules/session/rig_preset/CMakeLists.txt:5 links nlohmann PRIVATE although the public header needs it, and tests/CMakeLists.txt:247 records that as a known limitation.

#### C11. [medium] Repo-wide C++ naming and shape differ from the style skill, and siblings differ from each other

Evidence: Files are PascalCase (every file under include/, src/, modules/, apps/); constants are `kCamelCase` in 672 distinct names, e.g. apps/host_gui/AudioImportCache.cpp:53. Interfaces carry an `I` prefix and are structs: include/SushiDSP/core/IAudioNode.hpp:50, IAudioDevice.hpp:49, ICabinetHost.hpp, modules/nodes/room/include/SushiDSP/room/IRoomHost.hpp. Namespaces: `SushiDSP::room`, `SushiDSP::dd3`, `SushiDSP::dk` are lowercase, while capture, routing, cabinet and the four amps sit directly in `SushiDSP`. In apps/host_gui, five classes use `SushiDSP::host` (AudioImportCache.hpp:35) while RigController.hpp:53, SlotGraph.hpp:68, SessionTimeline.hpp:52, RigPersistence.hpp:41, RigTopology.hpp:83 and TopologyApplier.hpp:41 are in the global namespace and ui/ uses a bare `namespace ui`. Enums without a sized type: NodeDescriptor.hpp:34, AsioSampleConverter.hpp:34, ChannelSelection.hpp:35, WavWriter.hpp:39, Ne570Compander.hpp:35, MesaMark2c.hpp:150, PanelLibrary.hpp:36. Access specifiers are not indented (RigGraph.hpp:41). src/ and include/ are used where the skill names source/.

Recommendation: This is a mass rename touching every public header, so ask the owner whether SushiDSP converges on the skill or records an exemption. The namespace inconsistency and the unsized enums can be fixed without that decision.

Review: confirmed. PascalCase files, k-constants (zero K_), four I-prefixed struct interfaces, mixed namespaces (global RigController/SlotGraph vs SushiDSP::host, bare 'ui' in 55 files), unindented access specifiers at RigGraph.hpp:41 all present. Unsized enums are 8, not 7: Dd3Converter.hpp:38 (Dd3Mode) is missing from the list. The finding bundles five separate rules; the later wave should split it.

#### C12. [medium] RigGraph combines graph editing, compilation and real-time processing

Evidence: include/SushiDSP/core/RigGraph.hpp:63-131: add_node/connect/remove_node/disconnect (editing), compile() at :77 (topological sort and prepare), push_parameter_change() at :114 (control-to-audio ring), process() at :118 (RT render), plus channel adaptation rules at :58 and :137-152. src/core/RigGraph.cpp carries all of it.

Recommendation: The later wave should check whether a compiled, immutable render plan can be split from the editable topology; RtGraphSlot already swaps whole graphs, which suggests the seam.

Review: confirmed. RigGraph.hpp:58-152 declares editing, compile, push_parameter_change and process on one class; RigGraph.cpp is 551 lines. The recommendation is a question for the later wave rather than a fix, which is appropriate for a structural pass.

#### C13. [medium] Tests are not laid out per module, and app classes are tested by compiling app sources into the test binary

Evidence: 0 of 12 modules have a `tests/` folder; all 125 unit tests are in tests/functional/unit. tests/CMakeLists.txt:125-137 adds apps/host_gui/RigController.cpp, SlotGraph.cpp, SessionTimeline.cpp, RigPersistence.cpp and seven more app sources directly to `sushidsp_tests`; 17 unit test files include app headers. tests/functional/regression holds six golden comparisons (test_jcm800_regression.cpp etc.) named after products, not after defects, and tests/golden holds only the .f32 data and GoldenFile.hpp. No tests/integration exists. The header comment at tests/CMakeLists.txt:1-17 is a separator-framed paragraph.

Recommendation: Move unit tests beside their module, move the six golden comparisons under tests/golden, keep tests/regression for defect-named tests, and turn the host_gui logic that tests need into a library target the test binary links.

Review: confirmed. No tests/ folder under any module; 125 unit and 6 regression files; tests/CMakeLists.txt:120-137 compiles app sources into the test binary; 17 test files include app headers; the six regression files are product goldens; tests/CMakeLists.txt:1-17 is a separator-framed block. 'Move tests beside the module' is a layout decision the audit should have flagged for the owner.

#### C14. [medium] Device backends, GUI views and tools have no tests

Evidence: No test includes include/SushiDSP/device/AsioAudioDevice.hpp or Sdl2AudioDevice.hpp (src/device/AsioAudioDevice.cpp has 7 throws and a rethrowing catch at :337). No test includes apps/host_gui/Mp3Decoder.hpp or 15 ui/ view headers (RoomPanel.cpp is 697 lines, ArrangementView.cpp 655). tools/ has 11 Python files and no test, including tools/dsp/convert_hrtf.py (1,119 lines, 35 top-level definitions) and the four checkers.

Recommendation: Add device-labelled tests for the two backends (reported as skipped without the device), a decode test for Mp3Decoder with a fixture, and pytest coverage for the checkers and convert_hrtf's pure functions.

Review: confirmed. No test references AsioAudioDevice, Sdl2AudioDevice or Mp3Decoder (Mp3Decoder.cpp is only compiled into the test binary as a dependency). tools/ holds 11 Python files and no test. AsioAudioDevice.cpp has 6 throw statements plus the rethrow at :337.

#### C15. [medium] CLI service layer writes to the terminal, holds module singletons and swallows broad exceptions

Evidence: cli/sushidsp/services/project.py:34 imports `console` and calls it at :158, :162, :180, :181, :226, :303, :310, :313; :90-91 create module-level `_RUNNER` and `_DRIVER`. `except Exception:` at :84 returns BuildType.debug silently; `except (SystemExit, Exception):` at :245 falls back to a path computed from `__file__`. Enum members are lowercase (`BuildType.debug`, :39-45) and module constants use `_UPPER` (`_BUILD_TYPE_FROM_CMAKE`, `_DOXYGEN_HINT`) instead of `K_`. There is no commands/ package; cli/sushidsp/cli.py holds all commands. cli/tests does not mirror the source path (e.g. test_build_type_resolution.py for services/project.py).

Recommendation: Have services return results and raise module errors, with the command layer printing; pass Runner and CMakeDriver in; catch the specific not-a-checkout error at :84 and :245. Check against the other Sushi CLIs so all are shaped alike.

Review: confirmed. project.py:34 imports console and uses it at the cited lines; _RUNNER/_DRIVER at :90-91; 'except Exception' at :84 and 'except (SystemExit, Exception)' at :245; lowercase enum members at :39-48; no commands/ package (cli.py is 176 lines).

#### C16. [medium] No function in production code is [[nodiscard]], and only four classes are final

Evidence: A search for 'nodiscard' over include/, src/, modules/ and apps/ returns zero lines, while the public headers there declare about 123 bool-returning functions (for example include/SushiDSP/core/RigGraph.hpp:114 push_parameter_change, modules/nodes/room/include/SushiDSP/room/RoomNode.hpp:114 set_scene and :116 set_hrtf, and the 12 bool+error signatures in apps/host_gui/RigController.hpp). 'final' appears on 4 lines against roughly 100 class and struct declarations. The audit put class-shape rules under not_checked, but the count is cheap and repo-wide.

Recommendation: Fold this into the F5 decision: whichever status model the owner picks, the wave adds [[nodiscard]] to every status-returning function in the same pass, and marks non-base classes final.

Review: found by review.

#### C17. [medium] The core library's own layering is unchecked, and core reaches up into io for a product-specific interface

Evidence: include/SushiDSP/core/ICabinetHost.hpp:26 includes SushiDSP/io/Ir.hpp, the single core -> io edge; the audit records it as a fact but not as a finding. ICabinetHost is a cabinet-product concept living in the foundation area, where IRoomHost sits in its own module (modules/nodes/room/include/SushiDSP/room/IRoomHost.hpp), so the two siblings are placed differently. tools/layering/check_layering.py:22 declares only K_TIER_ORDER = (nodes, catalog, session) and walks modules/ alone, so no rule covers include/SushiDSP areas (math, io, core, circuits, device, audio_format).

Recommendation: Ask the owner for the intended order of the core library areas, move ICabinetHost beside the cabinet module as IRoomHost is beside room (or move Ir below core), and extend the layering checker to the declared order.

Review: found by review.

#### C18. [medium] The registry is a global the app reaches from eight places, so dependencies do not arrive through constructors

Evidence: node_registry() (modules/catalog/node_registry/src/NodeRegistry.cpp:42) is a function-local static read directly at apps/host_gui/SlotGraph.cpp:58, :82, :101, RigPersistence.cpp:747, ui/GearBrowser.cpp:56-57, ui/RigChainView.cpp:140, ui/StagePanel.cpp:50 and modules/session/rig_preset/src/RigPreset.cpp:42. panel_table() in apps/host_gui/ui/PanelLibrary.cpp:122 is the same shape. The C++ style skill's class-shape rule 9 forbids globals and singletons as the way a dependency arrives. F8 treats only the duplicated scan, not the access path.

Recommendation: Pass a registry reference into SlotGraph, RigPersistence, the views and the rig_preset functions; then F8's find_descriptor becomes a method on that one object and tests can inject a small registry.

Review: found by review.

#### C19. [low] Wdf is built into the library but no production code uses it

Evidence: include/SushiDSP/math/Wdf.hpp is included only by src/math/Wdf.cpp and tests/functional/unit/test_wdf.cpp; no file under modules/, apps/ or src/circuits references `Wdf`. CLAUDE.md in the repo mentions the WDF adaptor tree as deferred.

Recommendation: Ask the owner whether Wdf stays as a kept kernel for a planned circuit (then record it in docs/design/REMAINING_WORK.md) or is removed.

Review: confirmed. Wdf appears only in include/SushiDSP/math/Wdf.hpp, src/math/Wdf.cpp, tests/functional/unit/test_wdf.cpp, CMakeLists.txt and docs/design/SUSHIDSP.md.

#### C20. [low] room publishes 39 headers, including internal building blocks

Evidence: modules/nodes/room/include/SushiDSP/room/ holds 39 headers; DelayRing.hpp, FadeWindow.hpp, FirFilter.hpp, FrameDelay.hpp, GeometricPath.hpp, ModalPath.hpp, DirectPath.hpp and BinauralStage.hpp are not included by any test or by any file outside the module. apps include RoomNode, IRoomHost, RoomScene and related scene types.

Recommendation: Move headers used only inside the module to its private source folder so the public surface is the node, its host interface and the scene types.

Review: confirmed. 39 headers in the room include folder; none of the eight named headers is included from outside modules/nodes/room.

#### C21. [low] tools/ scripts raise bare RuntimeError/SystemExit and three lack the future import

Evidence: tools/spice/export.py:85, :102, :132, :154, :165, :196, :218, :277 raise `RuntimeError`; tools/dsp/design_halfband.py:61 raises `RuntimeError`; tools/dsp/design_resampler.py:187 and :189 raise `SystemExit` with a message. `from __future__ import annotations` is missing in tools/dsp/convert_hrtf.py, design_halfband.py, design_resampler.py and tools/spice/export.py. tools/dsp/convert_hrtf.py:899-900 skips an unreadable documentation file with `except OSError: continue`. bras_reference.py and motus_reference.py already define their own error classes, so the siblings differ.

Recommendation: Give export.py and the two design scripts a module error class as bras_reference.py has, add the future import, and report the skipped file in convert_hrtf.py.

Review: confirmed. All cited raise lines exist; the four tools files lack the future import (so do three near-empty cli files); convert_hrtf.py:899-900 is 'except OSError: continue'.

#### C22. [low] A test header lacks #pragma once and the tree has no .clang-format

Evidence: tests/common/TimingGate.hpp has no `#pragma once` (the only .hpp of 189 without it). `git ls-files` shows no `.clang-format` at the root, so the formatting rules the C++ style skill calls authoritative are not encoded; brace and access-specifier style already varies (see F11).

Recommendation: Add the pragma, and add the shared Sushi `.clang-format` once the owner settles F11.

Review: confirmed. tests/common/TimingGate.hpp is the only .hpp without #pragma once; no .clang-format is tracked.

### Not checked

- Line-by-line reading of any process() body; RT-safety of the audio path was not reviewed.
- The body of src/math (Resampler, SincResampler, Fft, PartitionedConvolution, StreamingConvolution, Oversampler, Biquad) and include/SushiDSP/math/dk/*: only includes, headers and file openings were sampled.
- src/circuits stages and models beyond their include edges and file headers.
- modules/nodes/room internals: 73 files, of which only RoomNode.cpp catch sites, HrtfSetLoader.cpp:236-250 and the header list were read.
- modules/nodes/dd3, cabinet, routing, tubescreamer sources beyond grep-level checks; diezel_vh4 and mesa_mark2c process() bodies.
- modules/nodes/capture beyond TrackRecorder.cpp catch sites.
- apps/host_gui/ui/* view files beyond PanelLibrary.cpp, and RigPersistence.cpp (14 throws) and SessionTimeline.cpp bodies.
- apps/host_asio/main.cpp and src/device/AsioAudioDevice.cpp beyond their catch sites; platform-portability rules.
- Per-symbol @brief coverage was sampled in core headers only, not counted across the tree; the repository's own tools/documentation/check_source_comments.py was not run (no project tool was run).
- Class-shape rules across the tree: `[[nodiscard]]`, `final`, `explicit`, rule of five, include group order.
- Test determinism: fixed seeds, float tolerances (69 unit files match a float-literal EXPECT_EQ pattern, not inspected), and the six unit tests that reference steady_clock/system_clock/sleep_for (test_audio_import_queue, test_band_response_fit, test_image_source_solver, test_rig_controller, test_room_render_set, test_rt_safety).
- Test name conventions beyond test_biquad.cpp; tests/benchmark/main.cpp (887 lines) and tests/measure headers.
- cli/sushidsp/config.py, env.py, console.py, services/diag.py and the cli/tests bodies; Python docstring shape across tools/.
- tools/layering/check_layering.py rules versus the measured include graph.
- CMake files, CI workflows, licence headers' wording, and everything under docs/ (other dimensions).
- third_party/ and apps/host_gui vendored code, build/ and other ignored folders.
