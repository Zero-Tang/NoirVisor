@echo off

:loop

cls
cargo clippy --all

echo Continue will restart clippy!
pause.

goto loop