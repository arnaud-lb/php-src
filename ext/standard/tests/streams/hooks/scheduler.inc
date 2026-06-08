<?php

use Io\Hooks\{Hooks, PollInfo, PollResult};
use Io\Poll\{Context, Event};

class Scheduler implements Hooks
{
    private Context $pollContext;
    private array $ready = [];
    private array $handles = [];         // spl_object_id(Handle) -> spl_object_id(Handle) (for poll() handles only)
    private array $fiberWatchers = [];   // fiberId -> [[Watcher, fdKey|null], ...]
    private array $fiberDeadlines = [];  // fiberId -> [deadline_ns, fiber, PollInfo|null]
    private array $fiberIsMulti = [];    // fiberId -> true (for poll_multi fibers)

    public function __construct()
    {
        $this->pollContext = new Context();
    }

    public function run(callable $main): void
    {
        $this->ready[] = [new Fiber($main), null];

        while ($this->ready !== [] || $this->fiberWatchers !== [] || $this->fiberDeadlines !== []) {
            $this->runReadyFibers();
            $this->pollFds();
        }
    }

    public function runReadyFibers(): void
    {
        while ($this->ready !== []) {
            [$fiber, $resumeValue] = array_shift($this->ready);
            $fiber->isSuspended() ? $fiber->resume($resumeValue) : $fiber->start();
        }
    }

    public function pollFds(): void
    {
        if ($this->fiberWatchers === [] && $this->fiberDeadlines === []) {
            return;
        }

        // Compute wait() timeout from the nearest deadline
        $timeoutSec = null;
        $timeoutUsec = 0;
        $now = hrtime(true);
        foreach ($this->fiberDeadlines as [$deadline]) {
            $remaining = $deadline - $now;
            if ($remaining <= 0) {
                $timeoutSec = 0;
                $timeoutUsec = 0;
                break;
            }
            $remainingUsec = intdiv($remaining, 1_000);
            if ($timeoutSec === null || $remainingUsec < $timeoutSec * 1_000_000 + $timeoutUsec) {
                $timeoutSec = intdiv($remainingUsec, 1_000_000);
                $timeoutUsec = $remainingUsec % 1_000_000;
            }
        }

        $watchers = $this->pollContext->wait($timeoutSec, $timeoutUsec);

        // Group ready PollResults by fiber before resuming anyone
        $fiberResults = [];
        foreach ($watchers as $watcher) {
            [$fiber] = $watcher->getData();
            $fiberId = spl_object_id($fiber);
            if (!isset($this->fiberWatchers[$fiberId])) {
                continue;
            }
            $result = new PollResult();
            $result->handle = $watcher->getHandle();
            $result->events = $watcher->getTriggeredEvents();
            $result->timeout = false;
            $fiberResults[$fiberId] ??= [$fiber, []];
            $fiberResults[$fiberId][1][] = $result;
        }

        foreach ($fiberResults as $fiberId => [$fiber, $results]) {
            if (!isset($this->fiberWatchers[$fiberId])) {
                continue;
            }
            $this->removeAllFiberWatchers($fiberId);
            unset($this->fiberDeadlines[$fiberId]);

            if ($this->fiberIsMulti[$fiberId] ?? false) {
                unset($this->fiberIsMulti[$fiberId]);
                $this->ready[] = [$fiber, $results];      // array of PollResult
            } else {
                $this->ready[] = [$fiber, $results[0]];   // single PollResult
            }
        }

        // Handle expired deadlines
        $now = hrtime(true);
        foreach ($this->fiberDeadlines as $fiberId => [$deadline, $fiber, $info]) {
            if ($deadline > $now) {
                continue;
            }
            $this->removeAllFiberWatchers($fiberId);
            unset($this->fiberDeadlines[$fiberId]);

            if ($this->fiberIsMulti[$fiberId] ?? false) {
                unset($this->fiberIsMulti[$fiberId]);
                $this->ready[] = [$fiber, []];            // empty array = timeout for poll_multi
            } else {
                $result = new PollResult();
                $result->handle = $info->handle;
                $result->events = $info->events;
                $result->timeout = true;
                $this->ready[] = [$fiber, $result];
            }
        }
    }

    private function removeAllFiberWatchers(int $fiberId): void
    {
        foreach ($this->fiberWatchers[$fiberId] as [$w, $fdKey]) {
            if ($fdKey !== null) {
                unset($this->handles[$fdKey]);
            }
            $w->remove();
        }
        unset($this->fiberWatchers[$fiberId]);
    }

    public function poll(PollInfo $info): PollResult
    {
        $fiber = Fiber::getCurrent();
        $fiberId = spl_object_id($fiber);

        if ($info->timeout_ms >= 0) {
            $deadline = hrtime(true) + $info->timeout_ms * 1_000_000;
            $this->fiberDeadlines[$fiberId] = [$deadline, $fiber, $info];
        }

        $id = spl_object_id($info->handle);
        if (isset($this->handles[$id])) {
            throw new \Exception("Handle already registered");
        }
        $this->handles[$id] = $id;
        $this->fiberWatchers[$fiberId] = [
            [$this->pollContext->add($info->handle, $info->events, [$fiber]), $id],
        ];

        /** @var PollResult $result */
        $result = Fiber::suspend();
        return $result;
    }

    public function poll_multi(?int $timeout_ms, PollInfo ...$infos): array
    {
        $fiber = Fiber::getCurrent();
        $fiberId = spl_object_id($fiber);
        $this->fiberWatchers[$fiberId] = [];
        $this->fiberIsMulti[$fiberId] = true;

        if ($timeout_ms !== null && $timeout_ms >= 0) {
            $deadline = hrtime(true) + $timeout_ms * 1_000_000;
            $this->fiberDeadlines[$fiberId] = [$deadline, $fiber, null];
        }

        foreach ($infos as $info) {
            $this->fiberWatchers[$fiberId][] = [
                $this->pollContext->add($info->handle, $info->events, [$fiber]),
                null,
            ];
        }

        /** @var PollResult[] $results */
        $results = Fiber::suspend();
        return $results ?? [];
    }

    public function go(callable $fn): void
    {
        $this->ready[] = [new Fiber($fn), null];
        $this->ready[] = [Fiber::getCurrent(), null];
        Fiber::suspend();
    }
}
