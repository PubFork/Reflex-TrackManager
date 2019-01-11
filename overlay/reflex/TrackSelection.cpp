#include "TrackSelection.h"
#include <chrono>
#include <fstream>
#include <turbojpeg.h>
#include "imgui/imgui.h"



const char* g_trackTypeComboItems[] =
{
	  "All Track Types"
	, "National"
	, "Supercross"
	, "FreeRide"
};

const char* g_slotComboItems[] =
{
	  "All Slots"
	, "1"
	, "2"
	, "3"
	, "4"
	, "5"
	, "6"
	, "7"
	, "8"
};

const char* g_sortByComboItems[] =
{
	  "Name"
	, "Slot"
	, "Type"
	, "Author"
	, "Date Created"
	, "Downloads"
	, "Favorite"
};

TrackSelection::TrackSelection(std::shared_ptr<TrackManagementClient> client)
	: m_trackManagementClient(client)
	, m_selectedTrackName("")
	, m_previouslySelectedTrackName("")
	, m_previewImage(nullptr)
	, m_previeImageWidth(0)
	, m_previewImageHeight(0)
	, m_trackTypeFilterIndex(0)
	, m_slotFilterIndex(0)
	, m_sortByIndex(1) // sort by slot by default
	, m_loadNewImage(false)
	, m_lastTimeStamp(0)
{
}

TrackSelection::~TrackSelection()
{
	if (m_previewImage != nullptr)
	{
		m_previewImage->Release();
		m_previewImage = nullptr;
	}
}

void TrackSelection::render(LPDIRECT3DDEVICE9 device)
{
	ImGui::SetNextWindowSize(ImVec2(960, 982), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(10, 49), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Track Selection", nullptr, ImGuiWindowFlags_NoResize))
	{
		static int previousTrackTypeIndex = 0;
		static int previousSlotTypeIndex = 0;
		static int previousSortByIndex = 0;

		trackmanagement::TrackRequest request;
		request.set_tracktype(g_trackTypeComboItems[m_trackTypeFilterIndex]);
		request.set_slot(g_slotComboItems[m_slotFilterIndex]);
		request.set_sortby(g_sortByComboItems[m_sortByIndex]);


		//GNARLY_TODO: try realtime updating back by implementing ImGuiListClipper.
		//This will allow us to fetch only the small amount of data we are currently viewing on screen. It will also save on memory costs
		const int TrackRefreshRate = 10; //seconds
		using namespace std::chrono;
		seconds currentTime = duration_cast<seconds>(system_clock::now().time_since_epoch());
		if (currentTime.count() > m_lastTimeStamp)
		{
			m_allTracks = m_trackManagementClient->getTracks(request);
			m_lastTimeStamp = currentTime.count() + TrackRefreshRate;
		}

		
		//Select first track in list anytime there is a change in the track list
		if (m_trackTypeFilterIndex != previousTrackTypeIndex
			|| m_slotFilterIndex != previousSlotTypeIndex
			|| m_sortByIndex != previousSortByIndex)
		{
			m_allTracks = m_trackManagementClient->getTracks(request);
			if (m_allTracks.size() > 0)
			{
				m_selectedTrackName = m_allTracks[0].name();
				m_selectedTrack = m_allTracks[0];
			}
			previousTrackTypeIndex = m_trackTypeFilterIndex;
			previousSlotTypeIndex = m_slotFilterIndex;
			previousSortByIndex = m_sortByIndex;
		}
		else if (m_allTracks.size() == 0)
		{
			m_allTracks = m_trackManagementClient->getTracks(request);
		}



		drawPreviewImage(device, m_selectedTrack);
		
		
		drawComboBoxes();
		drawTableBody(m_allTracks);
		
		std::string selected = m_selectedTrackName;
		auto trackIter = std::find_if(m_allTracks.begin(), m_allTracks.end(), [&selected](const trackmanagement::Track& obj) {return obj.name() == selected; });
		if (trackIter != m_allTracks.end())
		{
			m_selectedTrack = *trackIter;
			if (m_previouslySelectedTrackName != m_selectedTrackName)
			{
				m_loadNewImage = true;
			}
		}

		drawActionButtons();

	}
	ImGui::End();
	m_previouslySelectedTrackName = m_selectedTrackName;
}

void TrackSelection::drawPreviewImage(LPDIRECT3DDEVICE9 device, const trackmanagement::Track& selected)
{
	if (m_loadNewImage)
	{
		if (m_previewImage != nullptr)
		{
			m_previewImage->Release();
			m_previewImage = nullptr;
		}
		m_previewImage = createTextureFromFile(device, selected.image().c_str());
		m_loadNewImage = false;
	}

	ImVec2 windowSize = ImGui::GetWindowSize();
	static int imagePreviewWindowWidth = 656;
	static int imagePreviewWindowHeight = 376;

	ImGui::SetCursorPosX((windowSize.x / 2) - (imagePreviewWindowWidth / 2));
	ImGui::BeginChild("preview image", ImVec2(imagePreviewWindowWidth, imagePreviewWindowHeight), true);
	if (m_previewImage != nullptr)
	{
		ImGui::Image(m_previewImage, ImVec2(m_previeImageWidth, m_previewImageHeight));
	}
	ImGui::EndChild();

}

void TrackSelection::drawComboBoxes()
{
	ImVec2 windowSize = ImGui::GetWindowSize();
	static int filtersWidth = 656;
	static float filtersHeight = 75;
	ImGui::SetCursorPosX((windowSize.x / 2) - (filtersWidth / 2));
	ImGui::BeginChild("filters", ImVec2(filtersWidth, filtersHeight));

	ImGui::Combo("Track Type Filter", &m_trackTypeFilterIndex, g_trackTypeComboItems, IM_ARRAYSIZE(g_trackTypeComboItems));

	ImGui::Combo("Slot Filter", &m_slotFilterIndex, g_slotComboItems, IM_ARRAYSIZE(g_slotComboItems));

	ImGui::Combo("Sort By", &m_sortByIndex, g_sortByComboItems, IM_ARRAYSIZE(g_sortByComboItems));
	ImGui::EndChild();
}

void TrackSelection::drawTableBody(const std::vector<trackmanagement::Track>& tracks)
{
	static float height = 34.0f;
	static float tableWidth = 1024.0f;
	ImGui::SetNextWindowContentSize(ImVec2(tableWidth, 0.0f));
	ImGui::BeginChild("body", ImVec2(0, ImGui::GetFontSize() * height), true, ImGuiWindowFlags_HorizontalScrollbar);
	ImGui::Columns(8, "availabletracks");

	setTableColumnWidth();

	ImGui::Text("Name"); ImGui::NextColumn();
	ImGui::Text("Slot"); ImGui::NextColumn();
	ImGui::Text("Type"); ImGui::NextColumn();
	ImGui::Text("Author"); ImGui::NextColumn();
	ImGui::Text("Date Created"); ImGui::NextColumn();
	ImGui::Text("Installs"); ImGui::NextColumn();
	ImGui::Text("My Installs"); ImGui::NextColumn();
	ImGui::Text("Favorite");  ImGui::NextColumn();
	ImGui::Separator();

	for (auto& track : tracks)
	{
		if (ImGui::Selectable(track.name().c_str(), m_selectedTrackName == track.name(), ImGuiSelectableFlags_SpanAllColumns))
		{
			m_selectedTrackName = track.name();
		}
		bool hovered = ImGui::IsItemHovered();
		ImGui::NextColumn();

		ImGui::Text("%d", track.slot()); ImGui::NextColumn();
		ImGui::Text(track.type().c_str()); ImGui::NextColumn();
		ImGui::Text(track.author().c_str()); ImGui::NextColumn();
		ImGui::Text(track.date().c_str()); ImGui::NextColumn();
		ImGui::Text("%d", track.installs()); ImGui::NextColumn();
		ImGui::Text("%d", track.myinstalls()); ImGui::NextColumn();
		ImGui::Text(track.favorite() ? "true" : "false"); ImGui::NextColumn();
	}
	ImGui::EndChild();
}

void TrackSelection::drawActionButtons()
{
	ImVec2 windowSize = ImGui::GetWindowSize();

	//Hardcoding position because window is fixed size and i'm tired
	static float offsetX = 50;
	static float offsetY = 34.0f;

	ImGui::SetCursorPosX(offsetX);
	ImGui::SetCursorPosY(windowSize.y - offsetY);

	ImGui::BeginChild("actions");
	if (ImGui::Button("Install Random National Tracks"))
	{
		m_trackManagementClient->installRandomNationals();
	}
	ImGui::SameLine();
	if (ImGui::Button("Install Random Supercross Tracks"))
	{
		m_trackManagementClient->installRandomSupercross();
	}
	ImGui::SameLine();
	if (ImGui::Button("Install Random FreeRide Tracks"))
	{
		m_trackManagementClient->installRandomFreeRides();
	}
	ImGui::SameLine();
	if (ImGui::Button("Install Selected Track"))
	{
		m_trackManagementClient->installSelectedTrack(m_selectedTrackName.c_str());
	}
	ImGui::EndChild();
}

void TrackSelection::setTableColumnWidth()
{
	static const float nameWidth = 350.0f;
	static const float slotWidth = 54.0f;
	static const float typeWidth = 100.0f;
	static const float authorWidth = 128.0f;
	static const float dateWidth = 128.0f;
	static const float downloadsWidth = 84.0f;
	static float myInstalls = 86.0f;
	static const float favoriteWidth = 84.0f;

	ImGui::SetColumnWidth(0, nameWidth);
	ImGui::SetColumnWidth(1, slotWidth);
	ImGui::SetColumnWidth(2, typeWidth);
	ImGui::SetColumnWidth(3, authorWidth);
	ImGui::SetColumnWidth(4, dateWidth);
	ImGui::SetColumnWidth(5, downloadsWidth);
	ImGui::SetColumnWidth(6, myInstalls);
	ImGui::SetColumnWidth(7, favoriteWidth);
}

void TrackSelection::invalidateDeviceObjects()
{
	if (m_previewImage != nullptr)
	{
		m_previewImage->Release();
		m_previewImage = nullptr;
	}
	
}
void TrackSelection::createDeviceObjects(LPDIRECT3DDEVICE9 device)
{
	if (m_previewImage != nullptr)
	{
		m_previewImage->Release();
		m_previewImage = nullptr;
	}

	m_previewImage = createTextureFromFile(device, m_selectedTrack.image().c_str());
}

LPDIRECT3DTEXTURE9 TrackSelection::createTextureFromFile(LPDIRECT3DDEVICE9 device, const char* fileName)
{
	LPDIRECT3DTEXTURE9 d3dTexture = nullptr;
	if (INVALID_FILE_ATTRIBUTES == GetFileAttributes(fileName) && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		//File not found load default
	}
	else
	{
		auto jpegFile = fopen(fileName, "rb");
		unsigned char* jpegBuf = nullptr;
		long size = 0;
		int inSubsamp, inColorspace;
		unsigned long jpegSize;

		if (jpegFile == nullptr)
			return nullptr; //GNARLY_TODO: log error opening input file

		if (fseek(jpegFile, 0, SEEK_END) < 0 || ((size = ftell(jpegFile)) < 0) || fseek(jpegFile, 0, SEEK_SET) < 0)
			return nullptr; //GNARLY_TODO: log error determining input file size

		if (size == 0)
			return nullptr; //GNARLY_TODO: log error Input file contains no data

		jpegSize = static_cast<unsigned long>(size);

		if ((jpegBuf = (unsigned char *)tjAlloc(jpegSize)) == NULL)
			return nullptr; //GNARLY_TODO: log error allocating JPEG buffer

		if (fread(jpegBuf, jpegSize, 1, jpegFile) < 1)
		{
			tjFree(jpegBuf);
			return nullptr; //GNARLY_TODO: log error reading input file
		}

		fclose(jpegFile);
		jpegFile = nullptr;

		tjhandle tjInstance = tjInitDecompress();
		if (tjInstance == nullptr)
		{
			tjFree(jpegBuf);
			return nullptr; //GNARLY_TODO: log error initializing decompressor"
		}

		if (tjDecompressHeader3(tjInstance, jpegBuf, jpegSize, &m_previeImageWidth, &m_previewImageHeight, &inSubsamp, &inColorspace) < 0)
		{
			tjFree(jpegBuf);
			return nullptr; //GNARLY_TODO: log error reading JPEG header"
		}

		int pixelFormat = TJPF_BGRX;
		int pixelSize = tjPixelSize[pixelFormat];

		unsigned char* imgBuf = (unsigned char *)tjAlloc(m_previeImageWidth * m_previewImageHeight * pixelSize);
		if (imgBuf == nullptr)
		{
			tjFree(jpegBuf);
			return nullptr; //GNARLY_TODO: log error allocating uncompressed image buffer
		}

		if (tjDecompress2(tjInstance, jpegBuf, jpegSize, imgBuf, m_previeImageWidth, 0, m_previewImageHeight, pixelFormat, 0) < 0)
		{
			tjFree(jpegBuf);
			tjFree(imgBuf);
			return nullptr; //GNARLY_TODO: log error decompressing JPEG image
		}

		tjFree(jpegBuf);
		jpegBuf = nullptr;
		tjDestroy(tjInstance);
		tjInstance = nullptr;

		if (device->CreateTexture(m_previeImageWidth, m_previewImageHeight, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &d3dTexture, NULL) == D3D_OK)
		{
			D3DLOCKED_RECT destRect;
			if (d3dTexture->LockRect(0, &destRect, NULL, 0) == D3D_OK)
			{
				memcpy(destRect.pBits, imgBuf, m_previeImageWidth * m_previewImageHeight * pixelSize);
				d3dTexture->UnlockRect(0);
			}
		}

		tjFree(imgBuf);
	}
	return d3dTexture;
}
