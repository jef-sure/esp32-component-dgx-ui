#!/usr/bin/env python3
"""
test_hook.py <work directory> <dgx-ui directory> [<chip>]: dgx_ui_tree() and dgx_ui_names() of project_include.cmake,
on the program of test/hook/project, run inside an ESP-IDF environment (the shell its activate script set up).

The project is copied into the work directory, and dgx-ui into its ext/dgx-ui, from a checkout or from a package
unpacked (the tests, the examples and what was built left out), so that the copy is what the program takes and the
steps may touch it. On the linux target of ESP-IDF:

1. a clean build: the tool built once for the project, the eight files made in three components, the program run: the
   tree loaded on the made tables and the tree added by its made function the same, and the others as made; a.json
   holding editors' objects, at the top and in a widget; the program's header in a directory of its own, which only
   HEADERS puts on the made files' path; an appearance made in the second component, a tree made on it there and a
   tree and tables made on it in main, the main's tree loaded on those tables the same as added, of its looks and style;
2. a description edited: its files made again, no other, no configure, the tool not built again;
3. the tree renamed inside: the same files, the new function, no configure;
4. the tool's source touched: the tool built again, then every file made again; the same for DGX's dgx_colors.h, whose
   names of colours the tool is built with;
5. the appearance edited: its files made again and those made on it, in both components, no other, no configure; its
   accent made DGX's orange, written in look.c by DGX's name;
6. the configure refusals, each by its words: one file name twice in a component, one prefix in two components, a
   prefix not of C, one of dgx_, a names file whose name is not of C, a description's file name no file may have, one
   that begins with '-', a word not understood, an appearance no dgx_ui_appearance() made;
7. a description the loader refuses: the build stops with the tool's words, the loader's code, byte and name;
8. no compiler of the computer: the build stops with the words of dgx_ui_c.py;
9. a clean build by make, not by ninja, where the rules of a component that does not depend on the one whose rule
   builds the tool wait for the tool by its target;
10. the program built for the chip as well, so that the made files are compiled by its compiler.
"""
import os
import re
import shutil
import subprocess
import sys

here = os.path.dirname(os.path.abspath(__file__))
work, dgx_ui = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
chip = sys.argv[3] if len(sys.argv) > 3 else "esp32"
project = os.path.join(work, "hook")
failures = []
IDF_PY = [os.environ.get("IDF_PYTHON", "python"), os.path.join(os.environ["IDF_PATH"], "tools", "idf.py")]
TOOL_BUILT = "dgx-ui: dgx_ui_c built for the computer"
MADE = {"a": "dgx-ui: pages/a.json made C, a.c and a.h", "b": "dgx-ui: b.json made C, other.c and other.h",
        "c": "dgx-ui: c.json made C, c.c and c.h", "d": "dgx-ui: d.json made C, third_page.c and third_page.h",
        "vocab": "dgx-ui: vocab.txt made the loader's tables, vocab.c and vocab.h", "look": "dgx-ui: look.json made C, look.c and look.h",
        "framed": "dgx-ui: framed.json made C, framed.c and framed.h", "themed": "dgx-ui: vocab.txt made the loader's tables, themed.c and themed.h"}


def idf(*args, build="build", target="linux", configure=False, env=None, timeout=1800, generator=None):
    """idf.py in the project. A call that configures, by a -D or for a new build directory, names the target and the
    build directory's own sdkconfig: the project's CMakeLists takes the computer's linker by the target, and idf.py
    removes the cache of a configure that failed. A build alone names nothing, as any -D configures again."""
    defines = [f"-DIDF_TARGET={target}", f"-DSDKCONFIG={os.path.join(project, build, 'sdkconfig')}"]
    configure = configure or any(arg.startswith("-D") for arg in args)
    run = subprocess.run(IDF_PY + (["-G", generator] if generator else []) + ["-B", build] + (defines if configure else []) + list(args), cwd=project,
                         capture_output=True, text=True, timeout=timeout, env=env)
    return run.returncode, run.stdout + run.stderr


def program(build="build"):
    elf = os.path.join(project, build, "hook.elf")
    if not os.path.exists(elf):
        return 1, "no program built"
    run = subprocess.run([elf], capture_output=True, text=True, timeout=60)
    return run.returncode, run.stdout


def edit(path, old, new):
    path = os.path.join(project, path)
    with open(path) as source:
        text = source.read()
    assert old in text, (path, old)
    with open(path, "w") as source:
        source.write(text.replace(old, new))


def check(step, held, said=""):
    if not held:
        failures.append(f"{step}: {said[-3000:]}")


def built_and_ran(step, output, status, made, tool, tree):
    """a build: the files made of `made` and no other, the tool built `tool` times, no configure; the program's words"""
    check(f"{step}: the build", status == 0, output)
    check(f"{step}: the tool built {output.count(TOOL_BUILT)} times, not {tool}", output.count(TOOL_BUILT) == tool, output)
    for key, words in MADE.items():
        check(f"{step}: {key} made {output.count(words)} times", output.count(words) == (1 if key in made else 0), output)
    check(f"{step}: configured again", "Configuring done" not in output or step == "1", output)
    ran, said = program()
    check(f"{step}: the program", ran == 0 and "hook: ok" in said and f'the tree "{tree}"' in said, said)


# the project and the dgx-ui it takes, afresh
shutil.rmtree(project, ignore_errors=True)
shutil.copytree(os.path.join(here, "project"), project)
shutil.copytree(dgx_ui, os.path.join(project, "ext", "dgx-ui"),
                ignore=shutil.ignore_patterns(".git", "build", "build-*", "managed_components", "examples", "test", "dist", "*.tgz", ".cache"))

# 1. clean
status, output = idf("build", configure=True)
built_and_ran("1", output, status, set(MADE), 1, "sound")

# 2. a description edited
edit("main/pages/a.json", '"pad": 4', '"pad": 6')
status, output = idf("build")
built_and_ran("2", output, status, {"a"}, 0, "sound")

# 3. the tree renamed inside
edit("main/pages/a.json", '"name": "sound"', '"name": "noise"')
edit("main/main.c", "sound_add(", "noise_add(")
status, output = idf("build")
built_and_ran("3", output, status, {"a"}, 0, "noise")
made_header = os.path.join(project, "build", "esp-idf", "main", "dgx_ui_made", "a.h")
check("3: a.h declares noise_add()", os.path.exists(made_header) and "int noise_add(dgx_ui_t *ui, int parent);" in open(made_header).read())

# 4. the tool's source touched
tool_source = os.path.join(project, "ext", "dgx-ui", "tools", "dgx_ui_c.c")
os.utime(tool_source)  # now: the copy keeps the checkout's older time
status, output = idf("build")
built_and_ran("4", output, status, set(MADE), 1, "noise")
colors_header = os.path.join(project, "managed_components", "jef-sure__dgx", "include", "dgx_colors.h")
check("4: DGX's dgx_colors.h", os.path.exists(colors_header), colors_header)
if os.path.exists(colors_header):
    os.utime(colors_header)
    status, output = idf("build")
    built_and_ran("4 dgx_colors.h", output, status, set(MADE), 1, "noise")

# 5. the appearance edited: its files and those made on it
edit("components/second/look.json", '"accent": "#f0a03c"', '"accent": "#ffa500"')
status, output = idf("build")
built_and_ran("5", output, status, {"look", "c", "framed", "themed"}, 0, "noise")
made_look = os.path.join(project, "build", "esp-idf", "second", "dgx_ui_made", "look.c")
look_text = open(made_look).read() if os.path.exists(made_look) else ""
check("5: look.c writes the accent DGX_ORANGE(DGX_RGB_24)", "0x44556a, DGX_ORANGE(DGX_RGB_24)};" in look_text, look_text[:2000])

# 6. the configure refusals
for case, words in [("files_twice", "dgx_ui_tree(more/a.json): a.c and a.h are made of pages/a.json already in this component: give one of them a PREFIX"),
                    # in the order ESP-IDF takes the components: the second's first
                    ("prefix_twice", "dgx_ui_tree(b.json): the prefix other is taken already, by c.json of second: two of it would collide at link"),
                    ("prefix_not_c", 'dgx_ui_tree(b.json): PREFIX "1x" is not a name of C'),
                    ("prefix_dgx", 'dgx_ui_tree(b.json): PREFIX "dgx_x" begins with dgx_, as the libraries\' names and headers do'),
                    ("names_not_c", 'dgx_ui_names(my-vocab.txt): the names file\'s name "my-vocab" is not a name of C: give its tables a PREFIX'),
                    ("file_no_name", 'dgx_ui_tree(odd page.json): "odd page" is no name for the files made: give the tree a PREFIX'),
                    ("file_dash", 'dgx_ui_tree(-page.json): "-page" is no name for the files made: give the tree a PREFIX'),
                    ("word", 'dgx_ui_tree(b.json): "PREFX other2" not understood: PREFIX <prefix>, NAMES <names file>, APPEARANCE <appearance>, HEADERS <header>...'),
                    ("appearance_unmade", "dgx_ui_tree(framed.json): ../components/third/d.json is not made by dgx_ui_appearance() before: call it in this "
                                          "component or in one it requires")]:
    status, output = idf(f"-DHOOK_CASE={case}", "reconfigure")
    check(f"6: {case}", status != 0 and words in re.sub(r"\s+", " ", output), output)

# 7. a description the loader refuses
status, output = idf("-DHOOK_CASE=refused", "build")
with open(os.path.join(project, "main", "bad.json")) as bad:
    byte = bad.read().index('"loudness"')
check("7: the loader's words", status != 0 and f'bad.json: byte {byte}: DGX_UI_LOAD_UNKNOWN "loudness"' in output, output)
status, output = idf("-DHOOK_CASE=", "reconfigure")
check("7: configured back", status == 0, output)

# 8. no compiler of the computer
shutil.rmtree(os.path.join(project, "build", "dgx_ui_c"), ignore_errors=True)
status, output = idf("build", env=dict(os.environ, CC="/nonexistent/cc"))
check("8: the script's words", status != 0 and "dgx_ui_c.py: no C compiler of the computer (/nonexistent/cc; set CC to another)" in output, output)
status, output = idf("build")
built_and_ran("8", output, status, set(MADE), 1, "noise")

# 9. by make, as ESP-IDF may build: the rules of the trees in other components wait for the tool's target
status, output = idf("build", build="build-make", configure=True, generator="Unix Makefiles")
ran, said = program("build-make")
check("9: the build by make", status == 0 and output.count(TOOL_BUILT) == 1 and ran == 0 and "hook: ok" in said, output + said)

# 10. for a chip
status, output = idf("build", build="build-chip", target=chip, configure=True)
check(f"10: the build for {chip}", status == 0 and output.count(TOOL_BUILT) == 1, output)

for failure in failures:
    print("  " + failure, file=sys.stderr)
print(f"test_hook.py: 10 steps held, dgx-ui of {dgx_ui}" if not failures else f"test_hook.py: {len(failures)} failed")
sys.exit(1 if failures else 0)
