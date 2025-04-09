use std::borrow::Cow;

static CDEF_FILES: [&str;4] = [
    "targets/targets.cdef",
    "targets/etca/etca.cdef",
    "targets/x86/x86.cdef",
    "ir/ops.cdef"
];

static LIB_C_FILES: [&str;46] = [
    "targets/targets.c",
    "ir/verify.c",
	"ir/fastalloc.c",
    "ir/dump.c",
    "ir/lifetimes.c",
    "ir/transform.c",
    "ir/builder.c",
    "ir/ir.c",
    "ir/eval.c",
    "ir/analysis.c",
    "s-expr/sexpr.c",
    "s-expr/ir_serial.c",
    "ir/passes.c",
    "ir/opt/tailcall.c",
    "ir/opt/loop_simplify.c",
    "ir/opt/reduce_loops.c",
    "ir/opt/vars.c",
    "ir/opt/constant_eval.c",
    "ir/opt/inline_vars.c",
    "ir/opt/reduce_if.c",
    "ir/opt/cmov.c",
    "ir/opt/comparisions.c",
    "ir/opt/dce.c",
    "ir/opt/join_compute.c",
    "ir/opt/ll_jumps.c",
    "ir/opt/ll_binary.c",
    "ir/opt/simple_patterns.c",
    "ir/opt/if_opts.c",
    "ir/opt/swap_if_cases.c",
    "ir/transform/single_assign_conditional.c",
    "ir/transform/single_assign.c",
    "ir/transform/normalize.c",
    "ir/transform/share_slots.c",
    "ir/transform/ssair_llir_lower.c",
    "ir/transform/cmov_expand.c",
    "ir/transform/ll_finalize.c",
    "ir/transform/lower_loops.c",
    "ir/transform/ll_if_invert.c",
    "ir/verify_cir.c",
    "ir/verify_common.c",
    "ir/verify_ssa.c",
    "targets/targets.c",
    "targets/etca/etca.c",
    "targets/x86/x86.c",
    "targets/x86/cg.c",
    "targets/x86/llir_conditionals.c",
];

fn main() {
    let mut c_files: Vec<Cow<'static, str>> = LIB_C_FILES.iter()
        .map(|x| (*x).into())
        .collect();

    for file in CDEF_FILES.iter() {
        std::fs::create_dir_all(format!("build/{}",std::path::Path::new(file).parent().unwrap().to_str().unwrap())).unwrap();
        c_files.push(rust_cdef::generate(file).into());
    }

    let mut lib = cc::Build::new();
    for x in c_files.into_iter() {
        lib.file(x.as_ref());
    }
    lib.warnings(false);
    lib.extra_warnings(false);
    lib.flag("-Wno-deprecated-declarations"); // need strdup
    lib.define("_CRT_SECURE_NO_WARNINGS", None);
    if !(lib.get_compiler().is_like_gnu() || lib.get_compiler().is_like_clang()) {
        eprintln!("It is not recommended to use non gcc-like (or clang) compilers with this. Consider using an LLVM Rust toolchain instead.");
    }
    lib.compile("ir");

    let out_path = std::path::PathBuf::from(std::env::var("OUT_DIR").unwrap());
    bindgen::builder()
        .allowlist_recursively(false)
        .header("rust_bindings.h")
        .blocklist_file(".*(stdio|stdlib|assert|string).h")
        .allowlist_file(".*(ir|.cdef|targets|sexpr).h")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("unable to gen bindings")
        .write_to_file(out_path.join("bindings.rs"))
        .expect("could nto write bindngs");
}
