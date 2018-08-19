#include <stdlib.h>
#include "gamepad.h"
#include "gb_ini.h"
#include "stb_image.h"
#include "utils.h"

typedef struct
{
	Gamepad* gamepad;
	ParseError* error;
} ParseState;

Button* findOrCreateButton(Gamepad* gamepad, const char* buttonName)
{
	for (int i = 0; i < gamepad->numButtons; ++i)
	{
		Button* button = &gamepad->buttons[i];
		if (STR_EQUAL(buttonName, button->name))
		{
			return button;
		}
	}

	if (gamepad->numButtons == MAX_BUTTONS) { return NULL; }

	Button* button = &gamepad->buttons[gamepad->numButtons];
	++gamepad->numButtons;
	memset(button, 0, sizeof(Button));
	strcpy(button->name, buttonName);

	return button;
}

bool LoadButtonImage(const char* path, Button* button)
{
	int width, height, comp;
	// Loading the image in 32-bit saves us from having to align scanlines
	// ourself
	stbi_uc* image = stbi_load(path, &width, &height, &comp, 4);
	if (image == NULL) { return false; }

	// Create a DIB section to write to
	HDC hdc = CreateIC("TouchJoy", "TouchJoy", NULL, NULL);

	BITMAPINFO bmi;
	memset(&bmi, 0, sizeof(bmi));;
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = -height; // Top down image
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	stbi_uc* out;
	HBITMAP bitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void **) &out, NULL, 0);

	// Transfer pixels to the DIB
	if (bitmap)
	{
		// CreateDIBSection expects bgra instead of rgba so we have to swizzle
		for (int i = 0; i < width * height; ++i)
		{
			stbi_uc r = image[i * 4 + 0];
			stbi_uc g = image[i * 4 + 1];
			stbi_uc b = image[i * 4 + 2];

			out[i * 4 + 0] = b;
			out[i * 4 + 1] = g;
			out[i * 4 + 2] = r;
		}

		button->image = bitmap;
		button->width = width;
		button->height = height;
		button->colorKey = RGB(image[0], image[1], image[2]);
	}

	DeleteDC(hdc);

	stbi_image_free(image);

	return bitmap != NULL;
}

gb_Ini_HRT GamepadIniHandler(
	void* data,
	const char* section,
	const char* name,
	const char* value
)
{
#	define RETURN_ERROR(MSG) \
	do { \
		state->error->message = MSG; \
		return false; \
	} while (0, 0)

#	define ENSURE(COND, FAIL_MSG) if(!(COND)) { RETURN_ERROR(FAIL_MSG); }

	ParseState* state = (ParseState*)data;
	Gamepad* gamepad = state->gamepad;

	Button* button = findOrCreateButton(gamepad, section);

	ENSURE(button, "Too many buttons");

	if (STR_EQUAL(name, "x") || STR_EQUAL(name, "left"))
	{
		button->hMargin = TO_NUM(value);
		button->hAnchor = ANCHOR_LEFT;
	}
	else if (STR_EQUAL(name, "y") || STR_EQUAL(name, "top"))
	{
		button->vMargin = TO_NUM(value);
		button->vAnchor = ANCHOR_TOP;
	}
	else if (STR_EQUAL(name, "right"))
	{
		button->hMargin = TO_NUM(value);
		button->hAnchor = ANCHOR_RIGHT;
	}
	else if (STR_EQUAL(name, "bottom"))
	{
		button->vMargin = TO_NUM(value);
		button->vAnchor = ANCHOR_BOTTOM;
	}
	else if (STR_EQUAL(name, "keycode"))
	{
		ENSURE(button->type == BTN_KEY, "Invalid button property");
		button->extras.key.code = (WORD)TO_NUM(value);
	}
	else if (STR_EQUAL(name, "direction"))
	{
		ENSURE(button->type == BTN_WHEEL, "Invalid button property");

		if (STR_EQUAL(value, "up"))
		{
			button->extras.wheel.direction = 1;
		}
		else if (STR_EQUAL(value, "down"))
		{
			button->extras.wheel.direction = -1;
		}
		else
		{
			RETURN_ERROR("Invalid wheel direction");
		}
	}
	else if (STR_EQUAL(name, "amount"))
	{
		ENSURE(button->type == BTN_WHEEL, "Invalid button property");

		int amount = strtol(value, 0, 0);
		ENSURE(amount > 0, "Invalid scroll amount");

		button->extras.wheel.amount = amount;
	}
	else if (STR_EQUAL(name, "keycode_up"))
	{
		ENSURE(button->type == BTN_STICK, "Invalid button property");
		button->extras.stick.codes[STICK_UP] = (WORD)TO_NUM(value);
	}
	else if (STR_EQUAL(name, "keycode_down"))
	{
		ENSURE(button->type == BTN_STICK, "Invalid button property");
		button->extras.stick.codes[STICK_DOWN] = (WORD)TO_NUM(value);
	}
	else if (STR_EQUAL(name, "keycode_left"))
	{
		ENSURE(button->type == BTN_STICK, "Invalid button property");
		button->extras.stick.codes[STICK_LEFT] = (WORD)TO_NUM(value);
	}
	else if (STR_EQUAL(name, "keycode_right"))
	{
		ENSURE(button->type == BTN_STICK, "Invalid button property");
		button->extras.stick.codes[STICK_RIGHT] = (WORD)TO_NUM(value);
	}
	else if (STR_EQUAL(name, "threshold"))
	{
		ENSURE(button->type == BTN_STICK, "Invalid button property");
		button->extras.stick.threshold = ((float)TO_NUM(value)) / 100.f;
	}
	else if (STR_EQUAL(name, "image"))
	{
		ENSURE(LoadButtonImage(value, button), "Could not load image");
	}
	else if (STR_EQUAL(name, "type"))
	{
		if (STR_EQUAL(value, "quit"))
		{
			button->type = BTN_QUIT;
		}
		else if (STR_EQUAL(value, "key"))
		{
			button->type = BTN_KEY;
		}
		else if (STR_EQUAL(value, "wheel"))
		{
			button->type = BTN_WHEEL;
			button->extras.wheel.amount = 1;
		}
		else if (STR_EQUAL(value, "stick"))
		{
			button->type = BTN_STICK;
			button->extras.stick.threshold = 0.5f;
			button->extras.stick.codes[STICK_UP] = VK_UP;
			button->extras.stick.codes[STICK_DOWN] = VK_DOWN;
			button->extras.stick.codes[STICK_LEFT] = VK_LEFT;
			button->extras.stick.codes[STICK_RIGHT] = VK_RIGHT;
		}
		else
		{
			RETURN_ERROR("Invalid button type");
		}
	}
	else
	{
		RETURN_ERROR("Invalid button property");
	}

#	undef RETURN_ERROR
#	undef ENSURE

	return true;
}

bool LoadGamepad(const char* path, Gamepad* gamepad, ParseError* error)
{
	gamepad->numButtons = 0;

	ParseState state;
	state.gamepad = gamepad;
	state.error = error;
	gb_Ini_Error parseError = gb_ini_parse(path, GamepadIniHandler, &state);

	error->line = parseError.line_num;
	if (parseError.type != GB_INI_ERROR_HANDLER_ERROR)
	{
		error->message = gb_ini_error_string(parseError);
	}

	bool success = parseError.type == GB_INI_ERROR_NONE;
	// Release all resources created before
	if (!success) { FreeGamepad(gamepad); }

	return success;
}

void FreeGamepad(Gamepad* gamepad)
{
	for (int i = 0; i < gamepad->numButtons; ++i)
	{
		Button* button = &gamepad->buttons[i];
		if (button->image) { DeleteObject(button->image); }
	}

	gamepad->numButtons = 0;
}

int GetButtonX(Button* button)
{
	switch (button->hAnchor)
	{
	case ANCHOR_LEFT:
		return button->hMargin;
	case ANCHOR_RIGHT:
		return GetSystemMetrics(SM_CXSCREEN) - (button->hMargin + button->width);
	default:
		return 0;
	}
}

int GetButtonY(Button* button)
{
	switch (button->vAnchor)
	{
	case ANCHOR_TOP:
		return button->vMargin;
	case ANCHOR_BOTTOM:
		return GetSystemMetrics(SM_CYSCREEN) - (button->vMargin + button->height);
	default:
		return 0;
	}
}
