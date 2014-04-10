#include "HelloWorldScene.h"
#include "SimpleAudioEngine.h"

using namespace CocosDenshion;
USING_NS_CC;

HelloWorldHud *HelloWorld::_hud = NULL;

Scene* HelloWorld::createScene()
{
    auto scene = Scene::create();

    auto layer = HelloWorld::create();
	auto hud = HelloWorldHud::create();

	_hud = hud;
	_hud->setGameLayer(layer);

    scene->addChild(layer);
    scene->addChild(hud);

    return scene;
}

bool HelloWorldHud::init()
{
    if (!Layer::init())
    {
        return false;
    }

    auto visibleSize = Director::getInstance()->getVisibleSize();
    label = LabelTTF::create("0", "fonts/Marker Felt.ttf", 18.0f, Size(50, 20), TextHAlignment::RIGHT);
    label->setColor(Color3B(0, 0, 0));
    int margin = 10;
    label->setPosition(visibleSize.width - (label->getDimensions().width / 2) - margin * 3,
        label->getDimensions().height / 2 + margin);
    this->addChild(label);

	MenuItem *on = MenuItemImage::create("CloseNormal.png", "CloseSelected.png");
	MenuItem *off = MenuItemImage::create("CloseNormal.png", "CloseSelected.png");

	auto toggleItem = MenuItemToggle::createWithCallback(CC_CALLBACK_1(HelloWorldHud::projectileButtonTapped, this), off, on, NULL);
	auto toggleMenu = Menu::create(toggleItem, NULL);
	toggleMenu->setPosition(on->getContentSize().width * 5, on->getContentSize().height / 2);
	this->addChild(toggleMenu);

    return true;
}

void HelloWorldHud::numCollectedChanged(int numCollected)
{
	char showStr[20];
	sprintf(showStr, "%d", numCollected);
	label->setString(showStr);
}

void HelloWorldHud::projectileButtonTapped(Object *pSender)
{
	if (_gameLayer->getMode() == 1) {
		_gameLayer->setMode(0);
	}
	else {
		_gameLayer->setMode(1);
	}
}

bool HelloWorld::init()
{
    if ( !Layer::init() )
    {
        return false;
    }

	// Add sounds/effect
	SimpleAudioEngine::getInstance()->preloadEffect("error.mp3");
	SimpleAudioEngine::getInstance()->preloadEffect("item.mp3");
	SimpleAudioEngine::getInstance()->preloadEffect("step.mp3");
	SimpleAudioEngine::getInstance()->preloadEffect("wade.mp3");
	SimpleAudioEngine::getInstance()->playBackgroundMusic("background.mp3");
	SimpleAudioEngine::getInstance()->setBackgroundMusicVolume(0.1);

	// Load the tile map
    std::string file = "01.tmx";
    auto str = String::createWithContentsOfFile(FileUtils::getInstance()->fullPathForFilename(file.c_str()).c_str());
    _tileMap = TMXTiledMap::createWithXML(str->getCString(),"");
    _background = _tileMap->layerNamed("Background");
	_foreground = _tileMap->layerNamed("Foreground01");
	_layer01 = _tileMap->layerNamed("Layer01");
	_layer02 = _tileMap->layerNamed("Layer02");
	_layer03 = _tileMap->layerNamed("Layer03");
	_cross = _tileMap->layerNamed("CanCross01");
	_cross->setVisible(false);
	_blockage = _tileMap->layerNamed("Blockage01");
	_blockage->setVisible(false);
	
	addChild(_tileMap, -1);

	// Position of the player
	TMXObjectGroup *objects = _tileMap->getObjectGroup("Object-Player");
	CCASSERT(NULL != objects, "'Object-Player' object group not found");
	
	auto playerShowUpPoint = objects->getObject("PlayerShowUpPoint");
	CCASSERT(!playerShowUpPoint.empty(), "PlayerShowUpPoint object not found");

	int x = playerShowUpPoint["x"].asInt();
	int y = playerShowUpPoint["y"].asInt();

	_player = Sprite::create("029.png");
	_player->setPosition(x + _tileMap->getTileSize().width / 2, y + _tileMap->getTileSize().height / 2);
	_player->setScale(0.5);

	addChild(_player);

	setViewPointCenter(_player->getPosition());

	// Position of the enemy
	for (auto& eSpawnPoint: objects->getObjects()){
		ValueMap& dict = eSpawnPoint.asValueMap();
		if(dict["Enemy"].asInt() == 1){
			x = dict["x"].asInt();
			y = dict["y"].asInt();
			this->addEnemyAtPos(Point(x, y));
		}
	}


	// Event listener touch
	auto listener = EventListenerTouchOneByOne::create();
	listener->onTouchBegan = [&](Touch *touch, Event *unused_event)->bool {return true;};
	listener->onTouchEnded = CC_CALLBACK_2(HelloWorld::onTouchEnded, this);
	this->_eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);

	_numCollected = 0;
	_mode = 0;

	this->schedule(schedule_selector(HelloWorld::testCollisions));
	return true;
}

void HelloWorld::addEnemyAtPos(Point pos)
{
	auto enemy = Sprite::create("030.png");
	enemy->setPosition(pos);
	enemy->setScale(0.5);

	this->animateEnemy(enemy);
	this->addChild(enemy);

	_enemies.pushBack(enemy);
}

void HelloWorld::enemyMoveFinished(Object *pSender)
{
    Sprite *enemy = (Sprite *)pSender;

    this->animateEnemy(enemy);
}

// a method to move the enemy 10 pixels toward the player
void HelloWorld::animateEnemy(Sprite *enemy)
{
	auto actionTo1 = RotateTo::create(0, 0, 180);
	auto actionTo2 = RotateTo::create(0, 0, 0);
	auto diff = ccpSub(_player->getPosition(), enemy->getPosition());

    if (diff.x < 0) {
		enemy->runAction(actionTo2);
    }
	if (diff.x > 0) {
		enemy->runAction(actionTo1);
	}

    // speed of the enemy
    float actualDuration = 0.3f;

    // Create the actions
    auto position = (_player->getPosition() - enemy->getPosition()).normalize()*10;
    auto actionMove = MoveBy::create(actualDuration, position);
    auto actionMoveDone = CallFuncN::create(CC_CALLBACK_1(HelloWorld::enemyMoveFinished, this));
    enemy->runAction(Sequence::create(actionMove, actionMoveDone, NULL));
}

void HelloWorld::testCollisions(float dt)
{
	Vector<Sprite*> projectilesToDelete;

	// iterate through projectiles
	for (Sprite *projectile : _projectiles) {
		auto projectileRect = Rect(
			projectile->getPositionX() - projectile->getContentSize().width / 2,
			projectile->getPositionY() - projectile->getContentSize().height / 2,
			projectile->getContentSize().width,
			projectile->getContentSize().height);

		Vector<Sprite*> targetsToDelete;

		// iterate through enemies, see if any intersect with current projectile
		for (Sprite *target : _enemies) {
			auto targetRect = Rect(
				target->getPositionX() - target->getContentSize().width / 2,
				target->getPositionY() - target->getContentSize().height / 2,
				target->getContentSize().width,
				target->getContentSize().height);

			if (projectileRect.intersectsRect(targetRect)) {
				targetsToDelete.pushBack(target);
			}
		}

		// delete all hit enemies
		for (Sprite *target : targetsToDelete) {
			_enemies.eraseObject(target);
			this->removeChild(target);
		}

		if (targetsToDelete.size() > 0) {
			// add the projectile to the list of ones to remove
			projectilesToDelete.pushBack(projectile);
		}
		targetsToDelete.clear();
	}

	// remove all the projectiles that hit.
	for (Sprite *projectile : projectilesToDelete) {
		_projectiles.eraseObject(projectile);
		this->removeChild(projectile);
	}
	projectilesToDelete.clear();
}

Point HelloWorld::tileCoordForPosition(Point position)
{
	int x = position.x / _tileMap->getTileSize().width;
	int y = ((_tileMap->getMapSize().height * _tileMap->getTileSize().height) - position.y) / _tileMap->getTileSize().height;
	return Point(x, y);
}

void HelloWorld::setPlayerPosition(Point position)
{
	Point tileCoord = this->tileCoordForPosition(position);
	Point tileCoord02 = this->tileCoordForPosition(position);
	int TileGid02 = _cross->getTileGIDAt(tileCoord02);
	if(TileGid02) {
		auto properties = _tileMap->getPropertiesForGID(TileGid02).asValueMap();
		if(!properties.empty()) {
			auto water = properties["Wade"].asString();
			if("true" == water){
				SimpleAudioEngine::getInstance()->playEffect("wade.mp3");
				_player->setPosition(position);
				return;
			}
		}
	}
	int tileGid = _blockage->getTileGIDAt(tileCoord);
	if(tileGid) {
		auto properties = _tileMap->getPropertiesForGID(tileGid).asValueMap();
		if(!properties.empty()) {

			auto collision = properties["Blockage"].asString();
			if("true" == collision){
				SimpleAudioEngine::getInstance()->playEffect("error.mp3");
				return;
			}

			auto collectable = properties["Collectable"].asString();

			if("true" == collectable){
				_blockage->removeTileAt(tileCoord);
				_foreground->removeTileAt(tileCoord);

				SimpleAudioEngine::getInstance()->playEffect("item.mp3");

				this->_numCollected++;
				this->_hud->numCollectedChanged(_numCollected);
				_player->setPosition(position);
			    return;
				
			}
			
		}
	}
	SimpleAudioEngine::getInstance()->playEffect("step.mp3");
	_player->setPosition(position);
}

void HelloWorld::onTouchEnded(Touch *touch, Event *unused_event)
{
	auto actionTo1 = RotateTo::create(0, 0, 180);
	auto actionTo2 = RotateTo::create(0, 0, 0);
	if (_mode == 0) {
	    auto touchLocation = touch->getLocation();
	    touchLocation = this->convertToNodeSpace(touchLocation);

	    auto playerPos = _player->getPosition();
	    auto diff = touchLocation - playerPos;
	    if (abs(diff.x) > abs(diff.y)) {
		    if (diff.x > 0) {
			    playerPos.x += _tileMap->getTileSize().width / 2;
			    _player->runAction(actionTo2);
		    }
		    else {
			    playerPos.x -= _tileMap->getTileSize().width / 2;
			    _player->runAction(actionTo1);
		    }
	    }
	    else {
		    if (diff.y > 0) {
			    playerPos.y += _tileMap->getTileSize().height / 2;
		    }
		    else {
			    playerPos.y -= _tileMap->getTileSize().height / 2;
		    }
    	}

	    if (playerPos.x <= (_tileMap->getMapSize().width * _tileMap->getMapSize().width) &&
		    playerPos.y <= (_tileMap->getMapSize().height * _tileMap->getMapSize().height) &&
		    playerPos.y >= 0 &&
		    playerPos.x >= 0)
	    {
		    this->setPlayerPosition(playerPos);
		
    	}

	    this->setViewPointCenter(_player->getPosition());
    }   else {
		// code to throw ninja stars will go here
		// Find where the touch is
		auto touchLocation = touch->getLocation();
		touchLocation = this->convertToNodeSpace(touchLocation);

		// Create a projectile and put it at the player's location
		auto projectile = Sprite::create("bullet.png");
		projectile->setPosition(_player->getPosition());
		projectile->setScale(0.25);
		this->addChild(projectile);

		// Determine where we wish to shoot the projectile to
		int realX;

		// Are we shooting to the left or right?
		auto diff = touchLocation - _player->getPosition();
		if (diff.x > 0)
		{
			realX = (_tileMap->getMapSize().width * _tileMap->getTileSize().width) + 
				(projectile->getContentSize().width / 2);
		}
		else {
			realX = -(_tileMap->getMapSize().width * _tileMap->getTileSize().width) -
				(projectile->getContentSize().width / 2);
		}
		float ratio = (float)diff.y / (float)diff.x;
		int realY = ((realX - projectile->getPosition().x) * ratio) + projectile->getPosition().y;
		auto realDest = Point(realX, realY);

		// Determine the length of how far we're shooting
		int offRealX = realX - projectile->getPosition().x;
		int offRealY = realY - projectile->getPosition().y;
		float length = sqrtf((offRealX*offRealX) + (offRealY*offRealY));
		float velocity = 480 / 1; // 480pixels/1sec
		float realMoveDuration = length / velocity;

		// Move projectile to actual endpoint
		auto actionMoveDone = CallFuncN::create(CC_CALLBACK_1(HelloWorld::projectileMoveFinished, this));
		projectile->runAction(Sequence::create(MoveTo::create(realMoveDuration, realDest), actionMoveDone, NULL));

		_projectiles.pushBack(projectile);
	}
}

void HelloWorld::projectileMoveFinished(Object *pSender)
{
	Sprite *sprite = (Sprite *)pSender;
	_projectiles.eraseObject(sprite);
	this->removeChild(sprite);
}


void HelloWorld::setViewPointCenter(Point position){
	auto winSize = Director::getInstance()->getWinSize();

	int x = MAX(position.x, winSize.width / 2);
	int y = MAX(position.y, winSize.height / 2);
	x = MIN(x, (_tileMap->getMapSize().width * _tileMap->getTileSize().width) - winSize.width / 2);
	y = MIN(y, (_tileMap->getMapSize().height * _tileMap->getTileSize().height) - winSize.height / 2);
	auto actualPosition = Point(x, y);

	auto centerOfView = Point(winSize.width / 2, winSize.height / 2);
	auto viewPoint = centerOfView - actualPosition;
	this->setPosition(viewPoint);
}

//void HelloWorld::win()
//{
//    GameOverScene *gameOverScene = GameOverScene::create();
//    gameOverScene->getLayer()->getLabel()->setString("You Win!");
//    Director::getInstance()->replaceScene(gameOverScene);
//}

//void HelloWorld::lose()
//{
//    GameOverScene *gameOverScene = GameOverScene::create();
//    gameOverScene->getLayer()->getLabel()->setString("You Lose!");
//    Director::getInstance()->replaceScene(gameOverScene);
//}

//void HelloWorld::menuCloseCallback(Ref* pSender)
//{
//    Director::getInstance()->end();
//
//#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
//    exit(0);
//#endif
//}


 //   Size visibleSize = Director::getInstance()->getVisibleSize();
 //   Point origin = Director::getInstance()->getVisibleOrigin();

 //   /////////////////////////////
 //   // 2. add a menu item with "X" image, which is clicked to quit the program
 //   //    you may modify it.

 //   // add a "close" icon to exit the progress. it's an autorelease object
 //   auto closeItem = MenuItemImage::create(
 //                                          "CloseNormal.png",
 //                                          "CloseSelected.png",
 //                                          CC_CALLBACK_1(HelloWorld::menuCloseCallback, this));
 //   
	//closeItem->setPosition(Point(origin.x + visibleSize.width - closeItem->getContentSize().width/2 ,
 //                               origin.y + closeItem->getContentSize().height/2));

 //   // create menu, it's an autorelease object
 //   auto menu = Menu::create(closeItem, NULL);
 //   menu->setPosition(Point::ZERO);
 //   this->addChild(menu, 1);

 //   /////////////////////////////
 //   // 3. add your codes below...

 //   // add a label shows "Hello World"
 //   // create and initialize a label
 //   
 //   auto label = LabelTTF::create("Hello World", "Arial", 24);
 //   
 //   // position the label on the center of the screen
 //   label->setPosition(Point(origin.x + visibleSize.width/2,
 //                           origin.y + visibleSize.height - label->getContentSize().height));

 //   // add the label as a child to this layer
 //   this->addChild(label, 1);

 //   // add "HelloWorld" splash screen"
 //   auto sprite = Sprite::create("HelloWorld.png");

 //   // position the sprite on the center of the screen
 //   sprite->setPosition(Point(visibleSize.width/2 + origin.x, visibleSize.height/2 + origin.y));

 //   // add the sprite as a child to this layer
 //   this->addChild(sprite, 0);
 //   
 //   return true;