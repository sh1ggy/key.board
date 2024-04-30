fn main() {
    cc::Build::new()
        // .file("foo.c")
        .file("src/test.c")
        .compile("test");
    tauri_build::build()
}
