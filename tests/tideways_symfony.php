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
        private $resolver;

        public function __construct($resolver)
        {
            $this->resolver = $resolver;
        }

        public function handle()
        {
            $controller = array(new \Acme\DemoBundle\Controller\DefaultController(), 'indexAction');
            $args = $this->resolver->getArguments(null, $controller);

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

    class TraceableControllerResolver {
        private $resolver;

        public function __construct($resolver)
        {
            $this->resolver = $resolver;
        }

        public function getArguments($request, $controller) {
            return $this->resolver->getArguments($request, $controller);
        }
    }
}

namespace Acme\DemoBundle\Controller {
    class DefaultController {
        public function indexAction()
        {
        }
    }
}
