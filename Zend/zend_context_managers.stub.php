<?php

/** @generate-class-entries */

interface ContextManager
{
    public function enterContext(): mixed;

    public function exitContext(?\Throwable $e = null): ?bool;
}

final class ResourceContextManager implements ContextManager
{
    private readonly mixed $resource;

    public function __construct(mixed $resource) {}

    public function enterContext(): mixed {}

    public function exitContext(?\Throwable $e = null): ?bool {}
}
