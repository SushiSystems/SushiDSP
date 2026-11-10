# check_test_odr.py
# SushiSkills - https://github.com/SushiSystems/SushiSkills
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""Checks the C++ test sources of a repository for three bugs that show only once they are linked.

Rules: rule_duplicate_type (a type defined at namespace scope in two files), rule_suite_fixture_mix
(a suite name used by TEST and by a fixture macro), rule_unsafe_name (a test name that can hold a
character outside [A-Za-z0-9_]). The rules read SourceFile values only.
Usage: python tools/testing/check_test_odr.py [repository root] [--report] [--rule NAME]
"""

from __future__ import annotations

import re
import sys
from collections import defaultdict
from collections.abc import Iterable, Iterator, Sequence
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from checking.checker import Checker, Issue, run  # noqa: E402
from checking.source_file import SourceFile, line_of, read_files  # noqa: E402
from checking.tests_tree import discover  # noqa: E402

K_CPP_SUFFIXES = frozenset({".cpp", ".cc", ".cxx"})
K_NAMESPACE = re.compile(r"(?:^|\s)namespace\s*((?:\w+\s*::\s*)*\w+)?\s*$")
K_LINKAGE = re.compile(r'\bextern\s*"[^"]*"\s*$')
K_TYPE = re.compile(
    r"\b(?:struct|class|union)\s+(?:alignas\s*\([^)]*\)\s*)?(\w+)\s*(?:final\s*)?(?::(?!:)[^{;]*)?$"
)
K_CASE = re.compile(r"^[ \t]*(TEST|TEST_F|TEST_P|TYPED_TEST|TYPED_TEST_P)\s*\(\s*(\w+)\s*,", re.MULTILINE)
K_INSTANTIATE = re.compile(r"\bINSTANTIATE_(?:TEST_SUITE|TEST_CASE)_P\s*\(")
K_TO_STRING = re.compile(r"\bstd::to_string\s*\(")
K_PARAM_TYPE = re.compile(r"TestParamInfo\s*<\s*([\w:\s]+?)\s*>")
K_UNSIGNED_TYPE = re.compile(r"\b(?:size_t|unsigned|uint\d*_t|bool)\b")
K_PARAMETER_ARGUMENT = re.compile(r"\s*\w+\.(?:param|index)\s*")
K_NAME_MEMBER = re.compile(r"\bname\b|_name\b")
K_UNSIGNED_ARGUMENT = re.compile(r"\babs\b|\bunsigned\b|\bsize_t\b|\.size\s*\(\)|\bindex\b")
K_ROW_START = re.compile(r'^[ \t]*\{[ \t]*"([ ]+)"[ \t]*,', re.MULTILINE)
K_NAME_CHARACTERS = re.compile(r"[A-Za-z0-9_]*")
K_NAMESPACE_SCOPE = "namespace"
K_ANONYMOUS_SCOPE = "anonymous"
K_OTHER_SCOPE = "other"
K_OPENERS = "([{"
K_CLOSERS = ")]}"


@dataclass(frozen=True, slots=True)
class TypeDefinition:
    """Holds one type defined at namespace scope: where, and its qualified name."""

    path: Path
    line: int
    qualified: str


@dataclass(frozen=True, slots=True)
class TestSources:
    """Holds the C++ test files of one repository, with their text and their text without comments and literals."""

    files: tuple[SourceFile, ...]
    codes: tuple[str, ...]


def mask(text: str) -> str:
    """Returns the text with comments and the insides of string and character literals turned into spaces.

    Offsets and newlines stay, so a position in the result is a position in the text.
    """
    out = list(text)
    length = len(text)

    def erase(start: int, end: int) -> None:
        for position in range(start, min(end, length)):
            if out[position] != "\n":
                out[position] = " "

    index = 0
    while index < length:
        char = text[index]
        following = text[index + 1] if index + 1 < length else ""
        if char == "/" and following == "/":
            end = text.find("\n", index)
            end = length if end < 0 else end
            erase(index, end)
            index = end
        elif char == "/" and following == "*":
            end = text.find("*/", index + 2)
            end = length if end < 0 else end + 2
            erase(index, end)
            index = end
        elif char == '"' and index > 0 and text[index - 1] == "R":
            index = _erase_raw_string(text, index, erase)
        elif char == '"' or (char == "'" and not (index > 0 and text[index - 1].isalnum())):
            index = _erase_literal(text, index, erase)
        else:
            index += 1
    return "".join(out)


def _erase_raw_string(text: str, quote: int, erase) -> int:
    """Erases a raw string literal whose opening quote is at *quote*; returns the offset after it."""
    opening = text.find("(", quote)
    if opening < 0:
        return quote + 1
    closing = f'){text[quote + 1:opening]}"'
    end = text.find(closing, opening)
    end = len(text) if end < 0 else end + len(closing)
    erase(quote + 1, end - 1)
    return end


def _erase_literal(text: str, quote: int, erase) -> int:
    """Erases the inside of the literal whose opening quote is at *quote*; returns the offset after it."""
    index = quote + 1
    while index < len(text) and text[index] != text[quote] and text[index] != "\n":
        index += 2 if text[index] == "\\" else 1
    erase(quote + 1, index)
    return index + 1




def _opened_scope(header: str) -> tuple[str, str]:
    """Returns the kind and name of the scope a header opens: `namespace`, `anonymous` or `other`."""
    if K_LINKAGE.search(header):
        return K_NAMESPACE_SCOPE, ""
    match = K_NAMESPACE.search(header)
    if match is None:
        return K_OTHER_SCOPE, ""
    if match.group(1) is None:
        return K_ANONYMOUS_SCOPE, ""
    return K_NAMESPACE_SCOPE, re.sub(r"\s+", "", match.group(1))


def types_at_namespace_scope(source: SourceFile, code: str) -> list[TypeDefinition]:
    """Returns each type the file defines outside anonymous namespaces, classes and function bodies."""
    scopes: list[tuple[str, str]] = []
    found: list[TypeDefinition] = []
    start = 0
    for index, char in enumerate(code):
        if char in ";}":
            if char == "}" and scopes:
                scopes.pop()
            start = index + 1
        elif char == "{":
            header = code[start:index]
            opened = _opened_scope(header)
            if opened[0] == K_OTHER_SCOPE and all(kind == K_NAMESPACE_SCOPE for kind, _ in scopes):
                found.extend(_type_in(source, header, start, code, [name for _, name in scopes if name]))
            scopes.append(opened)
            start = index + 1
    return found


def _type_in(source: SourceFile, header: str, start: int, code: str,
             namespaces: Sequence[str]) -> Iterator[TypeDefinition]:
    """Yields the type a header defines, or nothing for an enum, an anonymous type or a specialisation."""
    match = K_TYPE.search(header)
    if match is None or header[:match.start()].rstrip().endswith("enum"):
        return
    yield TypeDefinition(source.path, line_of(code, start + match.start(1)),
                         "::".join([*namespaces, match.group(1)]))


def _call_arguments(code: str, opening: int) -> list[tuple[int, int]]:
    """Returns the (start, end) offsets of each top-level argument of the call whose `(` is at *opening*."""
    depth = 0
    spans: list[tuple[int, int]] = []
    first = opening + 1
    for index in range(opening, len(code)):
        char = code[index]
        if char in K_OPENERS:
            depth += 1
        elif char in K_CLOSERS:
            depth -= 1
            if depth == 0:
                spans.append((first, index))
                return spans
        elif char == "," and depth == 1:
            spans.append((first, index))
            first = index + 1
    return spans


def _function_body(code: str, name: str) -> tuple[int, int] | None:
    """Returns the offsets of the body of the first function called *name* in the code, if there is one."""
    match = re.search(rf"\b{re.escape(name)}\s*\([^;{{}}]*\)[^;{{}}]*\{{", code)
    if match is None:
        return None
    depth = 0
    for index in range(match.end() - 1, len(code)):
        depth += (code[index] == "{") - (code[index] == "}")
        if depth == 0:
            return match.end(), index
    return None


def _generators(code: str) -> Iterator[tuple[int, int]]:
    """Yields the offsets of each distinct name generator: the fourth argument of an instantiation, or the body it names."""
    seen: set[tuple[int, int]] = set()
    for call in K_INSTANTIATE.finditer(code):
        arguments = _call_arguments(code, call.end() - 1)
        if len(arguments) < 4:
            continue
        start, end = arguments[3]
        named = re.fullmatch(r"\s*&?\s*(\w+)\s*", code[start:end])
        body = _function_body(code, named.group(1)) if named else None
        span = body or (start, end)
        if span not in seen:
            seen.add(span)
            yield span


def _unsafe_generator_issues(source: SourceFile, code: str) -> Iterator[Issue]:
    """Yields an issue for each literal or `to_string` of a name generator that can break a test name."""
    for start, end in _generators(code):
        for literal in re.finditer(r'"( *)"', code[start:end]):
            at = start + literal.start() + 1
            content = source.text[at:at + len(literal.group(1))]
            if K_NAME_CHARACTERS.fullmatch(content) is None:
                yield Issue(source.path, line_of(code, at),
                            f'a name generator writes "{content}", which holds a character outside [A-Za-z0-9_]')
        for call in K_TO_STRING.finditer(code, start, end):
            closing = _call_arguments(code, call.end() - 1)[-1][1]
            argument = code[call.end():closing]
            if not (K_UNSIGNED_ARGUMENT.search(argument) or _is_unsigned_parameter(code[start:end], argument)):
                yield Issue(source.path, line_of(code, call.start()),
                            "a name generator takes std::to_string of a value that may be negative, "
                            "which writes a '-' into a test name")


def _is_unsigned_parameter(generator: str, argument: str) -> bool:
    """Returns whether an argument is the parameter of a generator that declares an unsigned parameter type."""
    declared = K_PARAM_TYPE.search(generator)
    return bool(declared and K_UNSIGNED_TYPE.search(declared.group(1))
                and K_PARAMETER_ARGUMENT.fullmatch(argument))


def _unsafe_row_issues(source: SourceFile, code: str) -> Iterator[Issue]:
    """Yields an issue for each row that starts with a name outside [A-Za-z0-9_], in a file whose generator reads a name."""
    if not any(K_NAME_MEMBER.search(code[start:end]) for start, end in _generators(code)):
        return
    for row in K_ROW_START.finditer(code):
        at = row.start(1)
        content = source.text[at:at + len(row.group(1))]
        if K_NAME_CHARACTERS.fullmatch(content) is None:
            yield Issue(source.path, line_of(code, at),
                        f'row name "{content}" holds a character outside [A-Za-z0-9_]')


def build_sources(files: Iterable[SourceFile]) -> TestSources:
    """Returns the C++ files among the given ones with their masked text."""
    kept = tuple(file for file in files if file.path.suffix in K_CPP_SUFFIXES)
    return TestSources(kept, tuple(mask(file.text) for file in kept))


def rule_duplicate_type(tree: TestSources) -> Iterator[Issue]:
    """Yields an issue for each type defined at namespace scope in two files under one qualified name."""
    places: dict[str, list[TypeDefinition]] = defaultdict(list)
    for source, code in zip(tree.files, tree.codes):
        for definition in types_at_namespace_scope(source, code):
            places[definition.qualified].append(definition)
    for qualified, definitions in places.items():
        if len({definition.path for definition in definitions}) < 2:
            continue
        for definition in definitions:
            others = ", ".join(f"{other.path.as_posix()}:{other.line}"
                               for other in definitions if other.path != definition.path)
            yield Issue(definition.path, definition.line,
                        f"{qualified} is defined here and in {others}; one definition rule: "
                        f"put it in an anonymous namespace or give it its own name")


def rule_suite_fixture_mix(tree: TestSources) -> Iterator[Issue]:
    """Yields an issue for each TEST whose suite name another test declares with a fixture."""
    cases: dict[str, list[tuple[Path, int, str]]] = defaultdict(list)
    for source, code in zip(tree.files, tree.codes):
        for match in K_CASE.finditer(code):
            cases[match.group(2)].append((source.path, line_of(code, match.start(1)), match.group(1)))
    for suite, uses in cases.items():
        fixtures = [use for use in uses if use[2] != "TEST"]
        if not fixtures:
            continue
        where = f"{fixtures[0][2]} at {fixtures[0][0].as_posix()}:{fixtures[0][1]}"
        for path, line, kind in uses:
            if kind == "TEST":
                yield Issue(path, line, f"suite {suite} uses TEST here and {where}; "
                                        f"GoogleTest needs one fixture class for a suite name")


def rule_unsafe_name(tree: TestSources) -> Iterator[Issue]:
    """Yields an issue for each row name or name generator that can yield a character outside [A-Za-z0-9_]."""
    for source, code in zip(tree.files, tree.codes):
        yield from _unsafe_row_issues(source, code)
        yield from _unsafe_generator_issues(source, code)


def collect(paths: list[Path]) -> Iterable[TestSources]:
    """Returns the C++ test sources of each given folder."""
    return [build_sources(read_files(discover(path).files, lambda file: file.suffix in K_CPP_SUFFIXES))
            for path in paths if path.is_dir()]


K_CHECKER = Checker(
    description="Checks the C++ test sources for a type defined twice, a suite name used with and without a "
                "fixture, and a test name that can hold a character gtest refuses.",
    collect=collect,
    rules={
        "rule_duplicate_type": rule_duplicate_type,
        "rule_suite_fixture_mix": rule_suite_fixture_mix,
        "rule_unsafe_name": rule_unsafe_name,
    },
)

if __name__ == "__main__":
    sys.exit(run(K_CHECKER))
