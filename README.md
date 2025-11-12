# Hotdog Manager

A multi-threaded producer-consumer program that simulates hotdog making and packing using pthreads.

## Compilation

```bash
gcc -std=c11 -o hotdog_manager hotdog_manager.c -lpthread
```

## Usage

```bash
./hotdog_manager <N> <S> <M> <P>
```

- **N**: Total hot dogs to produce
- **S**: Buffer size (must be less than N)
- **M**: Number of maker threads
- **P**: Number of packer threads (max 30)

### Example

```bash
./hotdog_manager 10 5 2 3
```

## Running Commands Multiple Times

### PowerShell (Windows)

**Method 1: For loop**
```powershell
for ($i=1; $i -le 5; $i++) { ./hotdog_manager.exe 10 5 2 3 }
```

**Method 2: Range operator**
```powershell
1..5 | ForEach-Object { ./hotdog_manager.exe 10 5 2 3 }
```

**Method 3: With run number display**
```powershell
1..5 | ForEach-Object { Write-Host "=== Run $_ ==="; ./hotdog_manager.exe 10 5 2 3 }
```

### Bash/Linux/Mac

**Method 1: Brace expansion**
```bash
for i in {1..5}; do ./hotdog_manager 10 5 2 3; done
```

**Method 2: C-style for loop**
```bash
for ((i=1; i<=5; i++)); do ./hotdog_manager 10 5 2 3; done
```

**Method 3: While loop**
```bash
i=1; while [ $i -le 5 ]; do ./hotdog_manager 10 5 2 3; i=$((i+1)); done
```

**Method 4: With run number display**
```bash
for i in {1..5}; do echo "=== Run $i ==="; ./hotdog_manager 10 5 2 3; done
```

## Output

The program generates a `log.txt` file containing:
- Order details (N, S, M, P)
- Production log (maker puts, packer gets)
- Summary statistics (per-maker and per-packer counts)
