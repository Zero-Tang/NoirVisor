@echo off

:loop

cls
cargo clippy

echo Continue will restart clippy!
pause.

goto loop