# BatchServe

<p align="center">
	<img src="https://readme-typing-svg.demolab.com?font=Fira+Code&weight=700&size=28&pause=1000&color=22C55E&center=true&vCenter=true&width=900&lines=Restaurant+Batch+Service+Simulation;POSIX+Threads+%2B+Semaphores;Fair+FIFO+Queue+%7C+No+Busy+Waiting+%7C+CLI+Animation" alt="BatchServe animated title" />
</p>

<p align="center">
	<img src="https://img.shields.io/badge/Language-C-0E7490?style=for-the-badge&logo=c&logoColor=white" alt="C badge" />
	<img src="https://img.shields.io/badge/Concurrency-Pthreads-0F766E?style=for-the-badge" alt="Pthreads badge" />
	<img src="https://img.shields.io/badge/Sync-Semaphores%20%2B%20Mutex-166534?style=for-the-badge" alt="Sync badge" />
	<img src="https://img.shields.io/badge/Queue-FIFO%20Fairness-15803D?style=for-the-badge" alt="FIFO badge" />
	<img src="https://img.shields.io/badge/Mode-CLI%20Animation-14532D?style=for-the-badge" alt="Animation badge" />
</p>

<p align="center">
	<img src="https://img.shields.io/badge/Status-Project%20Complete-16A34A?style=flat-square" alt="Project complete" />
	<img src="https://img.shields.io/badge/OS%20Concepts-Barrier%20Synchronization-0EA5E9?style=flat-square" alt="Barrier sync" />
	<img src="https://img.shields.io/badge/Batch%20Model-Strict%20N%20at%20a%20Time-0891B2?style=flat-square" alt="Strict batch" />
</p>

---

## Visual Overview

BatchServe simulates a restaurant serving diners in strict cycles:

1. Front door opens for exactly N diners.
2. Batch fills and service begins.
3. Back door opens after service.
4. All diners leave.
5. Next batch starts only after full exit.

It demonstrates core Operating Systems synchronization concepts with clean logs, fairness guarantees, starvation monitoring, and terminal animation.

---

## Architecture Flow

```mermaid
flowchart TD
		A[Diner Arrives] --> B[Get FIFO Ticket]
		B --> C[Wait Fair Queue Turn]
		C --> D[Wait Front Door Permit]
		D --> E[Enter Restaurant]
		E --> F{Inside Count == N?}
		F -- No --> G[Wait Back Door]
		F -- Yes --> H[Batch Full]
		H --> I[Serve Batch + CLI Animation]
		I --> J[Post Back Door N Times]
		J --> G
		G --> K[Leave Restaurant]
		K --> L{Inside Count == 0?}
		L -- No --> M[Thread Ends]
		L -- Yes --> N[Batch Completed]
		N --> O[Reopen Front Door for Next Batch]
		O --> M
```

---

## Feature Highlights

| Feature | What You See | Why It Matters |
|---|---|---|
| Colored Logs | Distinct colors for queue, enter, serve, leave, warnings | Easier to track concurrent behavior |
| Batch IDs | `[BATCH 01]`, `[BATCH 02]`, ... in logs | Clear phase grouping per cycle |
| FIFO Fairness Queue | Ticket-based diner ordering | Prevents random overtaking |
| Starvation Handling | Wait time printed; warning threshold alerts | Surfaces long waits and fairness health |
| CLI Animation | Spinner-based serving animation | Makes runtime state immediately visible |
| Strict Capacity | Maximum N diners inside | Enforces system constraint correctly |
| Barrier-like Batches | Next batch waits for complete exit | Correct cycle synchronization |

---

## Project Structure

```text
BatchServe/
|- main.c      # Full pthread + semaphore simulation
`- README.md   # Project documentation
```

---

## Build and Run

### Compile

```bash
gcc -pthread main.c -o restaurant
```

### Execute

```bash
./restaurant
```

On Windows (MSYS2/MinGW or WSL), run the equivalent executable command.

---

## Example Runtime Look

```text
Simulation started: 25 diners, batch size 5
Diner 7 queued with ticket 0
Diner 7 entered (Batch 1, inside=1)
...
[BATCH 01] Batch full, serving started
[BATCH 01] Serving diners... |
[BATCH 01] Serving diners... /
[BATCH 01] Serving diners... -
[BATCH 01] Serving finished.
[BATCH 01] Diner 7 leaving (remaining=4)
...
[BATCH 01] Batch completed
...
Simulation finished.
```

---

## Synchronization Model (Quick Notes)

- `front_door` semaphore controls how many diners can enter a batch.
- `back_door` semaphore blocks leaving until service ends.
- `fair_queue[]` semaphores implement FIFO ordering via tickets.
- `state_mutex` protects shared counters and batch transitions.
- `log_mutex` keeps multi-threaded logs visually clean.

---

## Learning Outcomes

This project is a practical OS lab for:

- Thread coordination with POSIX pthreads
- Semaphore-based gate control
- Mutex-protected shared state
- Batch synchronization (barrier-like lifecycle)
- Fair scheduling intuition and starvation visibility

---

## Author

Built for Operating Systems practice and demonstration.