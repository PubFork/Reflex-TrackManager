#include "Log.h"
#include "grpc/TrackManagementClient.h"
#include "imgui/imgui.h"


struct LogImpl
{
	ImGuiTextBuffer     Buf;
	ImGuiTextFilter     Filter;
	ImVector<int>       LineOffsets;        // Index to lines offset
	bool                ScrollToBottom;

	void    Clear() { Buf.clear(); LineOffsets.clear(); }

	void    AddLog(const char* fmt, ...) IM_FMTARGS(2)
	{
		int old_size = Buf.size();
		va_list args;
		va_start(args, fmt);
		Buf.appendfv(fmt, args);
		va_end(args);
		for (int new_size = Buf.size(); old_size < new_size; old_size++)
			if (Buf[old_size] == '\n')
				LineOffsets.push_back(old_size);
		ScrollToBottom = true;
	}

	void    Draw(const char* title, bool* p_open = NULL)
	{
		ImGui::SetNextWindowSize(ImVec2(934, 244), ImGuiCond_Always);
		ImGui::SetNextWindowPos(ImVec2(976, 787), ImGuiCond_Always);
		if (!ImGui::Begin(title, p_open, ImGuiWindowFlags_NoResize))
		{
			ImGui::End();
			return;
		}
		if (ImGui::Button("Clear")) Clear();
		ImGui::SameLine();
		bool copy = ImGui::Button("Copy");
		ImGui::SameLine();
		Filter.Draw("Filter", -100.0f);
		ImGui::Separator();
		ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
		if (copy) ImGui::LogToClipboard();

		if (Filter.IsActive())
		{
			const char* buf_begin = Buf.begin();
			const char* line = buf_begin;
			for (int line_no = 0; line != NULL; line_no++)
			{
				const char* line_end = (line_no < LineOffsets.Size) ? buf_begin + LineOffsets[line_no] : NULL;
				if (Filter.PassFilter(line, line_end))
					ImGui::TextUnformatted(line, line_end);
				line = line_end && line_end[1] ? line_end + 1 : NULL;
			}
		}
		else
		{
			ImGui::TextUnformatted(Buf.begin());
		}

		if (ScrollToBottom)
			ImGui::SetScrollHereY(1.0f);
		ScrollToBottom = false;
		ImGui::EndChild();
		ImGui::End();
	}
};

Log::Log(std::shared_ptr<TrackManagementClient> client)
	: m_trackManagementClient(client)

{
}

Log::~Log()
{
}

void Log::render()
{
	static LogImpl log;

	auto messages = m_trackManagementClient->getLogMessages();
	for (auto& message : messages)
	{
		const char* messageType = "Info";

		if (message.type() == trackmanagement::LogMessageType::LOG_WARNING)
		{
			messageType = "Warning";
		}
		else if (message.type() == trackmanagement::LogMessageType::LOG_ERROR)
		{
			messageType = "Error";
		}

		log.AddLog("[%s] %s\n", messageType, message.message().c_str());
	}
	log.Draw("Log", nullptr);
}
