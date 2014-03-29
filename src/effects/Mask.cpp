/**
 * @file
 * @brief Source file for Mask class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Also, if your software can interact with users remotely through a computer
 * network, you should also make sure that it provides a way for users to
 * get its source. For example, if your program is a web application, its
 * interface could display a "Source" link that leads users to an archive
 * of the code. There are many ways you could offer source, and different
 * solutions will be better for different programs; see section 13 for the
 * specific requirements.
 *
 * You should also get your employer (if you work as a programmer) or school,
 * if any, to sign a "copyright disclaimer" for the program, if necessary.
 * For more information on this, and how to apply and follow the GNU AGPL, see
 * <http://www.gnu.org/licenses/>.
 */

#include "../include/effects/Mask.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Mask::Mask() : reader(NULL) {

}

// Default constructor
Mask::Mask(ReaderBase *mask_reader, Keyframe mask_brightness, Keyframe mask_contrast) throw(InvalidFile, ReaderClosed) :
		reader(mask_reader), brightness(mask_brightness), contrast(mask_contrast)
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.name = "Alpha Mask / Wipe Transition";
	info.description = "Uses a grayscale mask image file to gradually wipe / transition between 2 images.";
	info.has_audio = false;
	info.has_video = true;

//	// Attempt to open mask file
//	try
//	{
//		// load image
//		mask = tr1::shared_ptr<Magick::Image>(new Magick::Image(path));
//		//mask->type(Magick::GrayscaleType); // convert to grayscale
//
//		// Remove transparency support (so mask will sub color brightness for alpha)
//		mask->matte(false); // This is required for the composite operator to copy the brightness of each pixel into the alpha channel
//
//	}
//	catch (Magick::Exception e) {
//		// raise exception
//		throw InvalidFile("File could not be opened.", path);
//	}
}

// Set brightness and contrast (brightness between -100 and 100)
void Mask::set_brightness_and_contrast(tr1::shared_ptr<Magick::Image> image, float brightness, float contrast)
{
	// Determine if white or black image is needed
	if (brightness >= -100.0 and brightness <= 0.0)
	{
		// Make mask darker
		double black_alpha = abs(brightness) / 100.0;
		tr1::shared_ptr<Magick::Image> black = tr1::shared_ptr<Magick::Image>(new Magick::Image(mask->size(), Magick::Color("Black")));
		black->matte(true);
		black->quantumOperator(Magick::OpacityChannel, Magick::MultiplyEvaluateOperator, black_alpha);
		image->composite(*black.get(), 0, 0, Magick::OverCompositeOp);

	}
	else if (brightness > 0.0 and brightness <= 100.0)
	{
		// Make mask whiter
		double white_alpha = brightness / 100.0;
		tr1::shared_ptr<Magick::Image> white = tr1::shared_ptr<Magick::Image>(new Magick::Image(mask->size(), Magick::Color("White")));
		white->matte(true);
		white->quantumOperator(Magick::OpacityChannel, Magick::MultiplyEvaluateOperator, white_alpha);
		image->composite(*white.get(), 0, 0, Magick::OverCompositeOp);
	}

	// Set Contrast
	image->sigmoidalContrast(true, contrast);

}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
tr1::shared_ptr<Frame> Mask::GetFrame(tr1::shared_ptr<Frame> frame, int frame_number)
{
	// Get the mask image (from the mask reader)
	mask = reader->GetFrame(frame_number)->GetImage();
	mask->type(Magick::GrayscaleType); // convert to grayscale
	mask->matte(false); // Remove transparency from the image. This is required for the composite operator to copy the brightness of each pixel into the alpha channel

	// Resize mask to match this frame size (if different)
	if (frame->GetImage()->size() != mask->size())
	{
		Magick::Geometry new_size(frame->GetImage()->size().width(), frame->GetImage()->size().height());
		new_size.aspect(true);
		mask->resize(new_size);
	}

	// Set the brightness of the mask (from a user-defined curve)
	set_brightness_and_contrast(mask, brightness.GetValue(frame_number), contrast.GetValue(frame_number));

	// Get copy of our source frame's image
	tr1::shared_ptr<Magick::Image> copy_source = tr1::shared_ptr<Magick::Image>(new Magick::Image(*frame->GetImage().get()));
	copy_source->channel(Magick::MatteChannel); // extract alpha channel as grayscale image
	copy_source->matte(false); // remove alpha channel
	copy_source->negate(true); // negate source alpha channel before multiplying mask
	copy_source->composite(*mask.get(), 0, 0, Magick::MultiplyCompositeOp); // multiply mask grayscale (i.e. combine the 2 grayscale images)

	// Copy the combined alpha channel back to the frame
	frame->GetImage()->composite(*copy_source.get(), 0, 0, Magick::CopyOpacityCompositeOp);

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
string Mask::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value Mask::JsonValue() {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = "Mask";
	root["brightness"] = brightness.JsonValue();
	root["contrast"] = contrast.JsonValue();
	//root["reader"] = reader.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Mask::SetJson(string value) throw(InvalidJSON) {

	// Parse JSON string into JSON objects
	Json::Value root;
	Json::Reader reader;
	bool success = reader.parse( value, root );
	if (!success)
		// Raise exception
		throw InvalidJSON("JSON could not be parsed (or is invalid)", "");

	try
	{
		// Set all values that match
		SetJsonValue(root);
	}
	catch (exception e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)", "");
	}
}

// Load Json::JsonValue into this object
void Mask::SetJsonValue(Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["brightness"].isNull())
		brightness.SetJsonValue(root["brightness"]);
	if (!root["contrast"].isNull())
		contrast.SetJsonValue(root["contrast"]);
}

