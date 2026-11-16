"""
static_autocoder_common:
    CLI scaffolding shared by the `static-*` autocoders (static-tlm-packetizer,
    static-cmd-dispatcher, ...). Each of those generates a single source file per
    deployment topology, keyed off a component instance carrying a specific
    annotation.

    These tools are the build-time half of an F Prime build autocoder. The
    configure-time half -- listing the files that will be generated -- is handled
    by `fpp-query` reading the same `--rules` file, so the two halves cannot
    disagree about a path.
"""

import argparse
import tomllib
from pathlib import Path
from typing import Callable, Optional
from collections.abc import Iterator

import fpp

#: Signature of a `generate` callback: renders the C++ source for the target
#: component instance found within a topology. `file_base` is the extension-free
#: name the document must use, derived from the rules file.
Generate = Callable[[fpp.Model, fpp.Topology, str], str]


def parse_arguments(tool_name: str) -> argparse.Namespace:
    """Parse the fpp-to-cpp-compatible command line"""
    parser = argparse.ArgumentParser(prog=tool_name)
    parser.add_argument("-d", "--directory", default=".", help="output directory")
    parser.add_argument(
        "-i", "--imports", default="", help="comma-separated files to import"
    )
    parser.add_argument(
        "--rules",
        metavar="<file>",
        required=True,
        help="fpp-query TOML rules naming the files to generate",
    )
    parser.add_argument("files", nargs="+", help="files to translate")
    return parser.parse_args()


def generated_suffix(rules: str) -> str:
    """The single suffix `rules` names, e.g. `StaticCmdDispatchAc.cpp`

    The rules file is the one artifact both halves of the autocoder read, so the
    paths `fpp-query` declares at configure time and the ones written here cannot
    drift -- a declared output that is never produced is a Ninja error well
    removed from its cause. These autocoders emit exactly one source per
    deployment topology, so any other count is a mistake in the rules file.
    """
    suffixes = [
        suffix
        for group in tomllib.loads(Path(rules).read_text()).get("group", [])
        for suffix in group.get("generate", [])
    ]
    if len(suffixes) != 1:
        raise ValueError(
            f"{rules}: expected exactly one `generate` suffix, found {suffixes}"
        )
    return suffixes[0]


def output_for(directory: str, node: fpp.ast.DefTopology, suffix: str) -> Path:
    """Path of the file generated for a DefTopology node"""
    return Path(directory) / f"{node.name}{suffix}"


def find_deployment_topology(model: fpp.Model) -> Optional[fpp.Topology]:
    for topology in model.analysis.topology_map.values():
        node = topology.node
        if node.in_source and node.is_deployment:
            return topology
    return None


def all_annotated_components(
    model: fpp.Model, annotation: str
) -> Iterator[fpp.Component]:
    """Yield all Components in model carrying `@ annotation`"""
    for component in model.analysis.component_map.values():
        if (
            annotation in component.node.pre_annotation
            or annotation in component.node.post_annotation
        ):
            yield component


def find_all_component_instances(
    topology: fpp.Topology, component: fpp.Component
) -> Iterator[fpp.InterfaceInstance.Component]:
    for ci in topology.component_instance_map:
        if ci.component and ci.component.node.node_id == component.node.node_id:
            yield ci


def get_singleton_instance(
    topology: fpp.Topology, component: fpp.Component
) -> Optional[fpp.InterfaceInstance.Component]:
    instances = list(find_all_component_instances(topology, component))
    if len(instances) > 1:
        raise fpp.DiagnosticError(
            fpp.Diagnostic(
                f"{component.symbol.qualified_name} must have at most one instance",
                children=[
                    fpp.DiagnosticMessage(
                        "instance defined here",
                        span=instance.node.span,
                        kind=fpp.DiagnosticMessageKind.Note,
                    )
                    for instance in instances
                ],
            )
        )
    elif len(instances) == 1:
        return instances[0]
    else:
        return None


def generate_all(
    options: argparse.Namespace,
    generate: Generate,
) -> int:
    """Write a source file for each deployment topology defined in the input files"""
    suffix = generated_suffix(options.rules)
    imports = [path for path in options.imports.split(",") if path]
    model = fpp.analyze(paths=options.files, imports=imports)
    for diag in model.diagnostics:
        print(diag.render(color=True))

    if model.has_errors:
        return 1

    topology = find_deployment_topology(model)
    # The same rules file gated this invocation at configure time: without a
    # deployment topology no output was declared, so nothing runs us here
    assert topology

    output_for(options.directory, topology.node, suffix).write_text(
        generate(model, topology, f"{topology.name}{Path(suffix).stem}")
    )
    return 0


def assert_fixed_width(ty: fpp.Type.Variant, message: str):
    match ty:
        case fpp.Type.String():
            raise fpp.DiagnosticError(fpp.Diagnostic(message))
        case fpp.Type.Struct() as s:
            for name, member in s.anon_struct.members:
                try:
                    assert_fixed_width(member, message)
                except fpp.DiagnosticError as e:
                    # Look up the member in the AST to get it's span
                    member_span = next(
                        (m.span for m in s.node.members if m.name == name), None
                    )
                    e.diagnostic.add_annotation("in struct member", span=member_span)
                    raise
        case fpp.Type.Array() as a:
            try:
                assert_fixed_width(a.anon_array.elt_type, message)
            except fpp.DiagnosticError as e:
                e.diagnostic.add_annotation(
                    "element of array", span=a.node.elt_type.span
                )
                raise
        case fpp.Type.Alias() as a:
            try:
                assert_fixed_width(a.alias_type, message)
            except fpp.DiagnosticError as e:
                e.diagnostic.add_annotation("aliased here", span=a.node.type_name.span)
                raise
        case _:
            # Everything else is fixed width (for now...)
            pass


def main(
    tool_name: str,
    generate: Generate,
) -> int:
    """Entry point shared by the static-* autocoder CLIs"""
    return generate_all(parse_arguments(tool_name), generate)
