--TEST--
Lazy objects: Lazy service initialization in dependency injection container
--FILE--
<?php

class EntityManager {
    public function __construct() {
        var_dump(__METHOD__);
    }
}

class Application {
    public function __construct(
        private EntityManager $em,
    ) {
        var_dump(__METHOD__);
    }

    public function doSomethingWithEntityManager()
    {
    }
}

class Container {
    public function getEntityManagerService(): EntityManager {
        $obj = (new ReflectionClass(EntityManager::class))->newInstanceWithoutConstructor();
        ReflectionLazyObject::makeLazyGhost($obj, function ($obj) {
            $obj->__construct();
        });
        return $obj;
    }

    public function getApplicationService(): Application {
        return new Application($this->getEntityManagerService());
    }
}

$container = new Container();

printf("Service can be fetched without initializing dependencies\n");
$application = $container->getApplicationService();
--EXPECTF--
Service can be fetched without initializing dependencies
string(24) "Application::__construct"
