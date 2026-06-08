<?php

/** @generate-class-entries */

namespace Io\Hooks {

    use Io\Poll\Handle;

    final class PollInfo {
        public Handle $handle;
        /* @var Io\Poll\Event[] */
        public array $events;
        public int $timeout_ms;
    }

    final class PollResult {
        public Handle $handle;
        /* @var Io\Poll\Event[] */
        public array $events;
        public bool $timeout;
    }

    interface Hooks {
        public function poll(PollInfo $info): PollResult;
        /* @return PollResult[] Empty when $timeout_ms is exceeded */
        public function poll_multi(?int $timeout_ms, PollInfo ...$info): array;
    }

    function set_hooks(?Hooks $hooks): ?Hooks {}
}
