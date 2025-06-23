{
    "targets": [
        {
            "target_name": "rbus_binding",
            "sources": ["src/rbus_binding.c"],
            "include_dirs": [
                "<!@(node -p \"require('node-addon-api').include\")",
                "/usr/include/rbus",
                "/usr/local/include/rbus",
                "/opt/homebrew/include/rbus",
            ],
            "libraries": ["-L/usr/lib -L/usr/local/lib -L/opt/homebrew/lib -lrbus"],
            "dependencies": ["<!(node -p \"require('node-addon-api').gyp\")"],
            "cflags": ["-g", "-O0"],
            "cflags_cc": ["-g", "-O0"],
            "xcode_settings": {
                "OTHER_CFLAGS": ["-g", "-O0"],
                "OTHER_CPLUSPLUSFLAGS": ["-g", "-O0"],
            },
        }
    ]
}
