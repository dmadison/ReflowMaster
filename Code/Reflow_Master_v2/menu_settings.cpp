#include "menu_settings.h"


template<>
SettingsOption* LinkedList<SettingsOption>::head = nullptr;

unsigned int SettingsPage::startingItem = 0;


SettingsOption::SettingsOption(const String& name, const String& desc, MenuGetFunc get, MenuSetFunc set, OptionMode m)
	:
	ItemName(name), ItemDescription(desc),
	getFunction(get), setFunction(set),
	Mode(m)
{
	LinkedList<SettingsOption>::add(this);
}

SettingsOption::~SettingsOption()
{
	LinkedList<SettingsOption>::remove(this);
}

unsigned long SettingsOption::getYPosition(unsigned int index) {
	return (ItemHeight * index) + ItemStartY;  // y coordinate on center
}

void SettingsOption::drawItem(unsigned int position) {
	const unsigned int xPos = ItemStartX;
	const unsigned int yPos = getYPosition(position);

	int16_t clearX, clearY;
	uint16_t clearWidth, clearHeight;

	tft.setTextSize(2);
	tft.getTextBounds(ItemName + " ", xPos, yPos, &clearX, &clearY, &clearWidth, &clearHeight);
	tft.fillRect(clearX, clearY, clearWidth, clearHeight, BLACK);  // clear text area

	tft.setCursor(xPos, yPos);

	tft.setTextColor(WHITE, BLACK);
	tft.print(ItemName);
	tft.print(' ');

	this->drawValue(position);
}

void SettingsOption::drawValue(unsigned int position) {
	const unsigned int yPos = getYPosition(position);

	int16_t nameX, dummyY;
	uint16_t nameWidth, dummyHeight;

	// Calculate size of item name to set cursor at end
	tft.setTextSize(2);
	tft.getTextBounds(ItemName + " ", ItemStartX, yPos, &nameX, &dummyY, &nameWidth, &dummyHeight);

	const unsigned int xPos = nameX + nameWidth;  // start at the end of the item name

	// Note that we're not reusing the variables from above because the program seems to get
	// confused and use the values from the first call rather than the second (potentially an optimization issue?)
	int16_t clearX, clearY;
	uint16_t clearWidth, clearHeight;

	// Calculate size of item description for clearing (using last value)
	tft.setTextSize(2);
	tft.getTextBounds(lastValue, xPos, yPos, &clearX, &clearY, &clearWidth, &clearHeight);
	tft.fillRect(clearX, clearY, clearWidth, clearHeight, BLACK);  // clear area

	tft.setCursor(xPos, yPos);
	tft.setTextColor(YELLOW, BLACK);
	tft.print(getFunction());

	lastValue = getFunction();  // save value so we know how much to 'clear' for next call
}

void SettingsOption::drawDescription() {
	const unsigned int height_area = 20;
	const unsigned int height_text = 16;

	tft.setTextSize(1);
	tft.setTextColor(GREEN, BLACK);
	tft.fillRect(0, tft.height() - height_area, tft.width(), height_area, BLACK);

	int textPosY = tft.height() - height_text;
	println_Center(tft, this->ItemDescription, tft.width() / 2, textPosY);
}

String SettingsOption::getModeString() const {
	switch (Mode)
	{
	case(OptionMode::Select):
		return "SELECT";
		break;
	case(OptionMode::Change):
		return "CHANGE";
		break;
	}
	return "";  // invalid
}


void SettingsPage::drawPage(unsigned int pos) {
	startingItem = 0;  // reset pagination
	updateScroll(pos);  // recalculate pagination for current position

	tft.fillScreen(BLACK);

	tft.setTextColor(BLUE, BLACK);
	tft.setTextSize(2);
	tft.setCursor(20, 20);
	tft.println("SETTINGS");

	drawItems();
	// drawScrollIndicator();  // skipping this because it's drawn over by the button icons anyways
}

void SettingsPage::redraw(unsigned int pos) {
	// drawing over *only* the items, rather than the entire page (title, cursor, button prompts)
	tft.fillRect(15, SettingsOption::getYPosition(0) - 5, 240, SettingsOption::getYPosition(ItemsPerPage), BLACK);

	// clear the description box as well
	tft.fillRect(0, tft.height() - 20, tft.width(), 20, BLACK);

	// recalculate pagination for current position (if necessary)
	updateScroll(pos);
	
	drawItems();
	drawScrollIndicator();
}

void SettingsPage::drawItems() {
	SettingsOption* ptr = SettingsOption::getHead();
	size_t position = 0;

	while (ptr != nullptr) {
		if (position >= startingItem) {
			ptr->drawItem(position - startingItem);
		}
		position++;
		ptr = ptr->getNext();

		if (position > startingItem + SettingsPage::ItemsPerPage - 1) break;  // break when we've filled the page (-1 for zero index)
	}
}

void SettingsPage::drawCursor(unsigned int pos) {
	SettingsOption* ptr = SettingsOption::getItemAtIndex(pos);
	if (ptr == nullptr) return;  // out of range

	// Clear cursor
	tft.fillRect(0, 20, 20, tft.height() - 20, BLACK);

	// If the current selection is not on the page, we need to redraw the list accordingly
	if (updateScroll(pos)) {
		redraw(pos);  // redraws those items on the screen
	}

	const unsigned int selectedPos = pos - startingItem;

	// Draw the actual cursor
	tft.setTextColor(BLUE, BLACK);
	tft.setTextSize(2);
	tft.setCursor(5, SettingsOption::getYPosition(selectedPos));
	tft.println(">");

	ptr->drawDescription();
}

void SettingsPage::changeOption(unsigned int pos) {
	SettingsOption* ptr = SettingsOption::getItemAtIndex(pos);
	if (ptr == nullptr) return;

	ptr->setFunction();

	switch (ptr->Mode) {
	case(OptionMode::Select):
		// do nothing here, we're loading a new menu instead
		break;
	case(OptionMode::Change):
		ptr->drawValue(SettingsOption::getPositionOf(ptr) - startingItem);
		break;
	}
}

bool SettingsPage::updateScroll(unsigned int pos) {
	if (onPage(pos)) return false;  // already on page, don't update

	switch (Scroll) {
	case(ScrollType::Smooth):
		if (pos < startingItem) {
			// 'decrement' to current as first
			startingItem = pos;
		}
		else if (pos > lastItem()) {
			// 'increment' to position as last
			startingItem = pos - ItemsPerPage + 1;  // +1 for 0 index
		}
		break;
	case(ScrollType::Paged):
	{
		const unsigned int Page = (pos / ItemsPerPage);  // page for this item
		const unsigned int PageStart = Page * ItemsPerPage;  // starting item for that page
		startingItem = PageStart;
		break;
	}
	}

	return true;
}

void SettingsPage::drawScrollIndicator() {
	if (SettingsOption::getCount() <= ItemsPerPage) return;  // no scrolling = no scroll indicator

	switch (Scroll) {
	case(ScrollType::Smooth):
	{
		const unsigned int NumPositions = SettingsOption::getCount() - ItemsPerPage;
		const float value = (float) startingItem / (float) NumPositions;
		
		DrawScrollIndicator(value, BLUE);
		break;
	}
	case(ScrollType::Paged):
	{
		const unsigned int Page = (startingItem / ItemsPerPage) + 1;
		const unsigned int NumItems = SettingsOption::getCount();
		const unsigned int NumPages = (NumItems / ItemsPerPage) + ((NumItems % ItemsPerPage != 0) ? 1 : 0);
		DrawPageIndicator(Page, NumPages, WHITE);
		break;
	}
	}
}

String SettingsPage::getButtonText(unsigned int pos) {
	SettingsOption* ptr = SettingsOption::getItemAtIndex(pos);
	if (ptr == nullptr) return "";
	return ptr->getModeString();
}

