<?php

use Io\Hooks\{Hooks, PollInfo, PollResult};
use Io\Poll\{Context, Event, Watcher};

class Scheduler implements Hooks
{
    private Context $pollContext;
    private array $ready = [];
    private array $watcherByHandle = [];      // handleId -> Watcher (persistent ET)
    private array $fiberByHandleEvent = [];   // handleId -> [eventName -> fiber]
    private array $pendingByHandle = [];      // handleId -> [Event, ...] (fired, not yet consumed)
    private array $registrationsByFiber = []; // fiberId -> [handleId -> [eventName, ...]]
    private array $fiberDeadlines = [];       // fiberId -> [deadline_ns, fiber, PollInfo|null]
    private array $fiberIsMulti = [];         // fiberId -> true

    public function __construct()
    {
        $this->pollContext = new Context();
        $this->pollContext->onWatcherRemoved($this->onWatcherRemoved(...));
    }

    private function onWatcherRemoved(Watcher $watcher): void
    {
        $handle = $watcher->getHandle();
        $handleId = spl_object_id($handle);

        // Handles can not be closed during polling, therefore no fiber is waiting for it
        assert(($this->fiberByHandleEvent[$handleId] ?? []) === []);

        unset($this->watcherByHandle[$handleId]);
        unset($this->fiberByHandleEvent[$handleId]);
        unset($this->pendingByHandle[$handleId]);
    }

    public function run(callable $main): void
    {
        $this->ready[] = [new Fiber($main), null];

        while ($this->ready !== [] || $this->registrationsByFiber !== [] || $this->fiberDeadlines !== []) {
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
        if ($this->registrationsByFiber === [] && $this->fiberDeadlines === []) {
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

        // Group triggered events by fiber; a fiber gets at most one result handle
        // (the first one that fires). Events from a second handle for the same fiber
        // go to pending for later consumption.
        $fiberResults = []; // fiberId -> [$fiber, PollResult]
        foreach ($watchers as $watcher) {
            $handle = $watcher->getHandle();
            $handleId = spl_object_id($handle);

            foreach ($watcher->getTriggeredEvents() as $event) {
                $fiber = $this->fiberByHandleEvent[$handleId][$event->name] ?? null;
                if ($fiber !== null) {
                    $fiberId = spl_object_id($fiber);
                    $existing = $fiberResults[$fiberId][1] ?? null;

                    if ($existing !== null && spl_object_id($existing->handle) !== $handleId) {
                        // Fiber already has a result from a different handle: defer to pending
                        $this->addPendingEvent($handleId, $event);
                    } else {
                        if ($existing === null) {
                            $result = new PollResult();
                            $result->handle = $handle;
                            $result->events = [];
                            $result->timeout = false;
                            $fiberResults[$fiberId] = [$fiber, $result];
                        }
                        $fiberResults[$fiberId][1]->events[] = $event;
                        // Terminal events persist in pending even when delivered to a fiber
                        if (self::isTerminalEvent($event)) {
                            $this->addPendingEvent($handleId, $event);
                        }
                    }
                } else {
                    $this->addPendingEvent($handleId, $event);
                }
            }
        }

        foreach ($fiberResults as $fiberId => [$fiber, $result]) {
            $this->clearFiberRegistrations($fiberId);
            unset($this->fiberDeadlines[$fiberId]);

            if ($this->fiberIsMulti[$fiberId] ?? false) {
                unset($this->fiberIsMulti[$fiberId]);
                $this->ready[] = [$fiber, [$result]];
            } else {
                $this->ready[] = [$fiber, $result];
            }
        }

        // Handle expired deadlines
        $now = hrtime(true);
        foreach ($this->fiberDeadlines as $fiberId => [$deadline, $fiber, $info]) {
            if ($deadline > $now) {
                continue;
            }
            $this->clearFiberRegistrations($fiberId);
            unset($this->fiberDeadlines[$fiberId]);

            if ($this->fiberIsMulti[$fiberId] ?? false) {
                unset($this->fiberIsMulti[$fiberId]);
                $this->ready[] = [$fiber, []];
            } elseif ($info === null) {
                // sleep deadline: resume with no value
                $this->ready[] = [$fiber, null];
            } else {
                $result = new PollResult();
                $result->handle = $info->handle;
                $result->events = $info->events;
                $result->timeout = true;
                $this->ready[] = [$fiber, $result];
            }
        }
    }

    private function clearFiberRegistrations(int $fiberId): void
    {
        foreach ($this->registrationsByFiber[$fiberId] ?? [] as $handleId => $eventNames) {
            foreach ($eventNames as $eventName) {
                unset($this->fiberByHandleEvent[$handleId][$eventName]);
            }
            if (empty($this->fiberByHandleEvent[$handleId])) {
                unset($this->fiberByHandleEvent[$handleId]);
            }
        }
        unset($this->registrationsByFiber[$fiberId]);
    }

    private function addOrModifyWatcher(int $handleId, object $handle, array $events): void
    {
        $eventsWithET = [...$events, Event::EdgeTriggered];

        if (isset($this->watcherByHandle[$handleId]) && $this->watcherByHandle[$handleId]->isActive()) {
            $watcher = $this->watcherByHandle[$handleId];
            $merged = array_values(array_unique([...$watcher->getWatchedEvents(), ...$eventsWithET], SORT_REGULAR));
            $watcher->modifyEvents($merged);
        } else {
            $this->watcherByHandle[$handleId] = $this->pollContext->add($handle, $eventsWithET);
        }
    }

    private static function isTerminalEvent(Event $event): bool
    {
        return $event === Event::Error || $event === Event::HangUp || $event === Event::ReadHangUp;
    }

    private function addPendingEvent(int $handleId, Event $event): void
    {
        if (!in_array($event, $this->pendingByHandle[$handleId] ?? [], true)) {
            $this->pendingByHandle[$handleId][] = $event;
        }
    }

    private function consumePending(int $handleId, object $handle, array $requestedEvents): ?PollResult
    {
        if (empty($this->pendingByHandle[$handleId])) {
            return null;
        }
        $matching = [];
        $remaining = [];
        foreach ($this->pendingByHandle[$handleId] as $pending) {
            if (in_array($pending, $requestedEvents, true)) {
                $matching[] = $pending;
                // Terminal events persist - every future poll sees them too
                if (self::isTerminalEvent($pending)) {
                    $remaining[] = $pending;
                }
            } else {
                $remaining[] = $pending;
            }
        }
        if (!$matching) {
            return null;
        }
        $this->pendingByHandle[$handleId] = $remaining;
        $result = new PollResult();
        $result->handle = $handle;
        $result->events = $matching;
        $result->timeout = false;
        return $result;
    }

    public function poll(PollInfo $info): PollResult
    {
        $fiber = Fiber::getCurrent();
        $fiberId = spl_object_id($fiber);
        $id = spl_object_id($info->handle);

        $result = $this->consumePending($id, $info->handle, $info->events);
        if ($result !== null) {
            return $result;
        }

        if ($info->timeout_ms >= 0) {
            $deadline = hrtime(true) + $info->timeout_ms * 1_000_000;
            $this->fiberDeadlines[$fiberId] = [$deadline, $fiber, $info];
        }

        $this->registrationsByFiber[$fiberId] = [];
        foreach ($info->events as $event) {
            $this->fiberByHandleEvent[$id][$event->name] = $fiber;
            $this->registrationsByFiber[$fiberId][$id][] = $event->name;
        }

        $this->addOrModifyWatcher($id, $info->handle, $info->events);

        /** @var PollResult $result */
        $result = Fiber::suspend();
        return $result;
    }

    public function pollMulti(?int $timeout_ms, PollInfo ...$infos): array
    {
        $fiber = Fiber::getCurrent();
        $fiberId = spl_object_id($fiber);
        $this->fiberIsMulti[$fiberId] = true;

        foreach ($infos as $info) {
            $id = spl_object_id($info->handle);
            $result = $this->consumePending($id, $info->handle, $info->events);
            if ($result !== null) {
                unset($this->fiberIsMulti[$fiberId]);
                return [$result];
            }
        }

        if ($timeout_ms !== null && $timeout_ms >= 0) {
            $deadline = hrtime(true) + $timeout_ms * 1_000_000;
            $this->fiberDeadlines[$fiberId] = [$deadline, $fiber, null];
        }

        $this->registrationsByFiber[$fiberId] = [];
        foreach ($infos as $info) {
            $id = spl_object_id($info->handle);
            foreach ($info->events as $event) {
                $this->fiberByHandleEvent[$id][$event->name] = $fiber;
                $this->registrationsByFiber[$fiberId][$id][] = $event->name;
            }
            $this->addOrModifyWatcher($id, $info->handle, $info->events);
        }

        /** @var PollResult[] $results */
        $results = Fiber::suspend();
        return $results ?? [];
    }

    public function sleep(int $seconds, int $nanoseconds): void
    {
        $fiber = Fiber::getCurrent();
        $fiberId = spl_object_id($fiber);
        $deadline = hrtime(true) + $seconds * 1_000_000_000 + $nanoseconds;
        $this->fiberDeadlines[$fiberId] = [$deadline, $fiber, null];
        Fiber::suspend();
    }

    public function go(callable $fn): void
    {
        $this->ready[] = [new Fiber($fn), null];
        $this->ready[] = [Fiber::getCurrent(), null];
        Fiber::suspend();
    }
}
