<?php

namespace Symfony\Component\HttpKernel {
    class Kernel
    {
        public function boot()
        {
        }
    }

    class HttpKernel
    {
        public function handle($method = 'indexAction')
        {
            $resolver = new \Symfony\Component\HttpKernel\Controller\ControllerResolver();
            $controller = array(new \Acme\DemoBundle\Controller\DefaultController(), $method);
            $event = new \Symfony\Component\HttpKernel\Event\FilterControllerEvent($this, $controller, null, null);
            $args = array();

            call_user_func_array($controller, $args);
        }
    }
}

namespace Symfony\Component\HttpKernel\Event {
    class FilterControllerEvent {
        public function __construct($kernel, $controller, $request, $type) {
        }
    }
}

namespace Symfony\Component\HttpKernel\Controller {
    class ControllerResolver {
        public function getArguments($request, $controller) {
            return array();
        }
    }
}

namespace Acme\DemoBundle\Controller {
    class DefaultController {
        public function indexAction()
        {
        }
        public function helloAction()
        {
        }
    }
 }
