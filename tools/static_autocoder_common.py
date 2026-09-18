"""
static_autocoder_common:
    CLI scaffolding shared by the `static-*` autocoders (static-tlm-packetizer,
    static-cmd-dispatcher, ...). Each of those generates a single source file per
    deployment topology, keyed off a component instance carrying a specific
    annotation.
"""

import argparse
from pathlib import Path
from typing import Callable, Optional
from collections.abc import Iterator

import fpp

#: Signature of a `generate` callback: renders the C++ source for the target
#: component instance found within a topology.
Generate = Callable[[fpp.Model, fpp.Topology], str]


def parse_arguments(tool_name: str) -> argparse.Namespace:
    """Parse the fpp-to-cpp-compatible command line"""
    parser = argparse.ArgumentParser(prog=tool_name)
    parser.add_argument("-d", "--directory", default=".", help="output directory")
    parser.add_argument(
        "-i", "--imports", default="", help="comma-separated files to import"
    )
    parser.add_argument(
        "--filenames",
        metavar="<file>",
        help="write the names of the files that would be generated to <file>, and exit",
    )
    parser.add_argument("files", nargs="+", help="files to translate")
    return parser.parse_args()


def output_for(directory: str, node: fpp.DefTopology, suffix: str) -> Path:
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
) -> Iterator[fpp.ComponentInterfaceInstance]:
    for ci in topology.component_instance_map:
        if ci.component and ci.component.node.node_id == component.node.node_id:
            yield ci


def get_singleton_instance(
    topology: fpp.Topology, component: fpp.Component
) -> Optional[fpp.ComponentInterfaceInstance]:
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
    suffix: str,
    generate: Generate,
) -> int:
    """Write a source file for each deployment topology defined in the input files"""
    imports = [path for path in options.imports.split(",") if path]
    model = fpp.analyze(paths=options.files, imports=imports)
    for diag in model.diagnostics:
        print(diag.render(color=True))

    if model.has_errors:
        return 1

    topology = find_deployment_topology(model)
    assert topology

    output_for(options.directory, topology.node, suffix).write_text(
        generate(model, topology)
    )
    return 0


def write_filenames(options: argparse.Namespace, suffix: str) -> int:
    """Write the paths `generate_all` would produce for the same inputs"""
    syntax = fpp.parse(paths=options.files)

    # Check if this module has a deployment topology
    class FilenamesVisitor(fpp.NodeVisitor):
        deployment_topology: Optional[fpp.DefTopology] = None

        def generic_visit(self, node: fpp.AstNode):
            # Shallow visitor
            pass

        def visit_DefModule(self, node: fpp.DefModule):
            # Deeply visit all modules to discover namespaced topologies
            super().generic_visit(node)

        def visit_DefTopology(self, node: fpp.DefTopology):
            self.deployment_topology = node

    visitor = FilenamesVisitor()
    visitor.visit(syntax)

    filenames: list[Path] = []

    if visitor.deployment_topology:
        filenames.append(
            output_for(
                options.directory, visitor.deployment_topology, suffix
            ).absolute()
        )

    with open(options.filenames, "w+") as f:
        f.writelines([str(m) for m in filenames])

    return 0


def main(
    tool_name: str,
    suffix: str,
    generate: Generate,
) -> int:
    """Entry point shared by the static-* autocoder CLIs"""
    options = parse_arguments(tool_name)

    if options.filenames:
        return write_filenames(options, suffix)
    else:
        return generate_all(options, suffix, generate)
