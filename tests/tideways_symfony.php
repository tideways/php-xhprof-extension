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
            $args = $resolver->getArguments(null, $controller);

            call_user_func_array($controller, $args);
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
