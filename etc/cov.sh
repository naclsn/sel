#!/bin/sh -eux
[ -d target/prof/ ] && rm -rf target/prof/*

export RUSTFLAGS=-Cinstrument-coverage LLVM_PROFILE_FILE=target/prof/sel-%p-%m.profraw
cargo build
cargo test

grcov target/prof/ --binary-path target/debug/ -s . -t html --branch --ignore-not-existing --ignore /\* -o target/prof/report-html
