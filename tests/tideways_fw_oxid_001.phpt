--TEST--
Tideways: Oxid Support
--FILE--
<?php

include __DIR__ . '/common.php';

class oxShopControl
{
    public function _process($sClass, $sFnc = null)
    {
        $oViewObject = new $sClass;
        $oViewObject->setClass($sClass);
        $oViewObject->setFncName($sFnc);
        $oViewObject->executeFunction($oViewObject->getFncName());
    }
}

class oxView
{
    protected $_sFnc;
    protected $_sClass;

    public function setFncName($fnc)
    {
        $this->_sFnc = $fnc;
    }

    public function setClass($sClass)
    {
        $this->_sClass = $sClass;
    }

    public function getFncName()
    {
        return $this->_sFnc;
    }

    public function executeFunction($name)
    {
        $this->$name();
    }
}

class alist extends oxView
{
    public function getArticleList()
    {
        $shop = new oxShopControl();

        for ($i = 0; $i < 3; $i++) {
            $shop->_process('article', 'getListItem');
        }
    }

    public function executeFunction($name)
    {
        return $this->getArticleList();
    }
}

class article extends oxView
{
    public function getListItem()
    {
    }
}

$shop = new oxShopControl();

tideways_enable();
$shop->_process('alist');

print_spans(tideways_get_spans());
echo "TX: " . tideways_transaction_name();
tideways_disable();

echo "\n\n";

tideways_enable();
$shop->_process('article', 'getListItem');
print_spans(tideways_get_spans());
echo "TX: " . tideways_transaction_name();

--EXPECTF--
app: 1 timers - 
php.ctrl: 3 timers - title=article::getListItem
php.ctrl: 1 timers - title=alist
TX: alist

app: 1 timers - 
php.ctrl: 1 timers - title=article::getListItem
TX: article::getListItem
