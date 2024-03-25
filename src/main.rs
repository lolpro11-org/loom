#[cxx::bridge]
mod ffi {
    // Any shared structs, whose fields will be visible to both languages.
    struct BlobMetadata {
        size: usize,
        tags: Vec<String>,
    }

    unsafe extern "C++" {
        include!("gtfs2graph/builder/Builder.h");

        // Zero or more opaque types which both languages can pass around but
        // only C++ can see the fields.
        type GTFSGraphBuilder;

        // Functions implemented in C++.
        fn consume();
        fn simplify();
    }
}

fn main() {
    println!("Hello, world!");
}
