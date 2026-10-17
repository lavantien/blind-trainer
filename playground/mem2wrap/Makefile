MEMGUARD := C:/Users/lavantien/dev/github/blind-trainer/scripts/memguard.ps1
SLEEPER := C:/Users/lavantien/dev/github/blind-trainer/target/release/bt-sleeper.exe

.PHONY: probe

probe:
	pwsh -NoProfile -ExecutionPolicy Bypass -File $(MEMGUARD) -Wrap mem2probe -WrapCommand '$(SLEEPER) 8'
