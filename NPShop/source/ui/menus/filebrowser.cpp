#include "ui/menus/filebrowser.hpp"
#include "ui/menus/homebrew.hpp"
#include "ui/sidebar.hpp"
#include "ui/option_box.hpp"
#include "ui/popup_list.hpp"
#include "ui/progress_box.hpp"
#include "ui/error_box.hpp"
#include "ui/menus/file_viewer.hpp"

#include "log.hpp"
#include "app.hpp"
#include "ui/nvg_util.hpp"
#include "fs.hpp"
#include "nro.hpp"
#include "defines.hpp"
#include "image.hpp"
#include "download.hpp"
#include "owo.hpp"
#include "swkbd.hpp"
#include "i18n.hpp"
#include "hasher.hpp"
#include "location.hpp"
#include "threaded_file_transfer.hpp"
#include "minizip_helper.hpp"

#include "yati/yati.hpp"
#include "yati/source/file.hpp"

#include <minIni.h>
#include <minizip/zip.h>
#include <minizip/unzip.h>
#include <dirent.h>
#include <cstring>
#include <cassert>
#include <string>
#include <string_view>
#include <ctime>
#include <span>
#include <utility>
#include <ranges>
#include <expected>

namespace npshop::ui::menu::filebrowser {
	namespace {

		constinit UEvent g_change_uevent;

		constexpr FsEntry FS_ENTRY_DEFAULT{
			"microSD card", "/", FsType::Sd, FsEntryFlag_Assoc,
		};

		constexpr FsEntry FS_ENTRIES[]{
			FS_ENTRY_DEFAULT,
			{ "Image System memory", "/", FsType::ImageNand },
			{ "Image microSD card", "/", FsType::ImageSd},
		};

		struct ExtDbEntry {
			std::string_view db_name;
			std::span<const std::string_view> ext;
		};

		constexpr std::string_view AUDIO_EXTENSIONS[] = {
			"mp3", "ogg", "flac", "wav", "aac" "ac3", "aif", "asf", "bfwav",
			"bfsar", "bfstm",
		};
		constexpr std::string_view VIDEO_EXTENSIONS[] = {
			"mp4", "mkv", "m3u", "m3u8", "hls", "vob", "avi", "dv", "flv", "m2ts",
			"m2v", "m4a", "mov", "mpeg", "mpg", "mts", "swf", "ts", "vob", "wma", "wmv",
		};
		constexpr std::string_view IMAGE_EXTENSIONS[] = {
			"png", "jpg", "jpeg", "bmp", "gif",
		};
		constexpr std::string_view INSTALL_EXTENSIONS[] = {
			"nsp", "xci", "nsz", "xcz",
		};
		// these are files that are already compressed or encrypted and should
		// be stored raw in a zip file.
		constexpr std::string_view COMPRESSED_EXTENSIONS[] = {
			"zip", "xz", "7z", "rar", "tar", "nca", "nsp", "xci", "nsz", "xcz"
		};
		constexpr std::string_view ZIP_EXTENSIONS[] = {
			"zip",
		};

		// case insensitive check
		auto IsSamePath(std::string_view a, std::string_view b) -> bool {
			return a.length() == b.length() && !strncasecmp(a.data(), b.data(), a.length());
		}

		auto IsExtension(std::string_view ext, std::span<const std::string_view> list) -> bool {
			for (auto e : list) {
				if (e.length() == ext.length() && !strncasecmp(ext.data(), e.data(), ext.length())) {
					return true;
				}
			}
			return false;
		}

		auto IsExtension(std::string_view ext1, std::string_view ext2) -> bool {
			return ext1.length() == ext2.length() && !strncasecmp(ext1.data(), ext2.data(), ext1.length());
		}

	} // namespace

	npshop::ui::menu::filebrowser::FsView::FsView(Menu* menu, const fs::FsPath& path, const FsEntry& entry, ViewSide side) : m_menu{ menu }, m_side{ side } {
		this->SetActions(
			std::make_pair(Button::L2, Action{ [this]() {
				if (!m_menu->m_selected.Empty()) {
					m_menu->ResetSelection();
				}

				// if both set, select all.
				if (App::GetApp()->m_controller.GotHeld(Button::R2)) {
					const auto set = m_selected_count != m_entries_current.size();

					for (u32 i = 0; i < m_entries_current.size(); i++) {
						auto& e = GetEntry(i);
						if (e.selected != set) {
							e.selected = set;
							if (set) {
								m_selected_count++;
							}
							 else {
							  m_selected_count--;
						  }
						}
						}
						}
						else {
						 GetEntry().selected ^= 1;
						 if (GetEntry().selected) {
							 m_selected_count++;
						 }
						else {
						 m_selected_count--;
						}
						}
						} }),

			std::make_pair(Button::A, Action{ "Open"_i18n, [this]() {
				if (m_entries_current.empty()) {
					return;
				}

				const auto& entry = GetEntry();

				if (entry.type == FsDirEntryType_Dir) {
					Scan(GetNewPathCurrent());
				}
			else {
					// special case for nro
					if (IsSd() && IsSamePath(entry.GetExtension(), "nro")) {
						App::Push<OptionBox>(
							"Launch "_i18n + entry.GetName() + '?',
							"No"_i18n, "Launch"_i18n, 1,
							[this](auto op_index) {
								if (op_index && *op_index) {
									nro_launch(GetNewPathCurrent());
								}
							}
						);
					}
				}
			} }),

			std::make_pair(Button::B, Action{ "Back"_i18n, [this]() {
				if (!m_menu->IsTab() && App::GetApp()->m_controller.GotHeld(Button::R2)) {
					m_menu->PromptIfShouldExit();
					return;
				}

				std::string_view view{ m_path };
				if (view != m_fs->Root()) {
					const auto end = view.find_last_of('/');
					assert(end != std::string_view::npos);

					if (end == 0) {
						Scan(m_fs->Root(), true);
					}
			else {
			 Scan(view.substr(0, end), true);
			}
			}
			else {
			 if (!m_menu->IsTab()) {
				 m_menu->PromptIfShouldExit();
			 }
			}
			} }),

			std::make_pair(Button::X, Action{ "Options"_i18n, [this]() {
				if (App::GetApp()->m_controller.GotHeld(Button::R2)) {
					DisplayAdvancedOptions();
				}
			else {
			 DisplayOptions();
			}
			} })
		);
		SetSide(m_side);

		auto buf = path;
		if (path.empty()) {
			ini_gets("paths", "last_path", entry.root, buf, sizeof(buf), App::CONFIG_PATH);
		}

		SetFs(buf, entry);
	}

	npshop::ui::menu::filebrowser::FsView::FsView(Menu* menu, ViewSide side) : FsView{ menu, "", FS_ENTRY_DEFAULT, side } {

	}

	FsView::~FsView() {
		// don't store mount points for non-sd card paths.
		if (IsSd()) {
			ini_puts("paths", "last_path", m_path, App::CONFIG_PATH);
		}
	}

	void FsView::Update(Controller* controller, TouchInfo* touch) {
		m_list->OnUpdate(controller, touch, m_index, m_entries_current.size(), [this](bool touch, auto i) {
			if (touch && m_index == i) {
				FireAction(Button::A);
			}
			else {
				SetIndex(i);
			}
			});
	}

	void FsView::Draw(NVGcontext* vg, Theme* theme) {
		const auto& text_col = theme->GetColour(ThemeEntryID_TEXT);

		if (m_entries_current.empty()) {
			gfx::drawTextArgs(vg, GetX() + GetW() / 2.f, GetY() + GetH() / 2.f, 36.f, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, theme->GetColour(ThemeEntryID_TEXT_INFO), "Empty..."_i18n.c_str());
			return;
		}

		constexpr float text_xoffset{ 15.f };
		bool got_dir_count = false;

		m_list->Draw(vg, theme, m_entries_current.size(), [this, text_col, &got_dir_count](auto* vg, auto* theme, auto v, auto i) {
			const auto& [x, y, w, h] = v;
			auto& e = GetEntry(i);

			auto text_id = ThemeEntryID_TEXT;
			const auto selected = m_index == i;
			if (selected) {
				text_id = ThemeEntryID_TEXT_SELECTED;
				gfx::drawRectOutline(vg, theme, 4.f, v);
			}
			else {
				if (i != m_entries_current.size() - 1) {
					gfx::drawRect(vg, Vec4{ x, y + h, w, 1.f }, theme->GetColour(ThemeEntryID_LINE_SEPARATOR));
				}
			}

			if (e.IsDir()) {
				DrawElement(x + text_xoffset, y + 5, 50, 50, ThemeEntryID_ICON_FOLDER);
			}
			else {
				auto icon = ThemeEntryID_ICON_FILE;
				const auto ext = e.GetExtension();
				if (IsExtension(ext, AUDIO_EXTENSIONS)) {
					icon = ThemeEntryID_ICON_AUDIO;
				}
				else if (IsExtension(ext, VIDEO_EXTENSIONS)) {
					icon = ThemeEntryID_ICON_VIDEO;
				}
				else if (IsExtension(ext, IMAGE_EXTENSIONS)) {
					icon = ThemeEntryID_ICON_IMAGE;
				}
				else if (IsExtension(ext, INSTALL_EXTENSIONS)) {
					// todo: maybe replace this icon with something else?
					icon = ThemeEntryID_ICON_NRO;
				}
				else if (IsExtension(ext, ZIP_EXTENSIONS)) {
					icon = ThemeEntryID_ICON_ZIP;
				}
				else if (IsExtension(ext, "nro")) {
					icon = ThemeEntryID_ICON_NRO;
				}

				DrawElement(x + text_xoffset, y + 5, 50, 50, icon);
			}

			if (e.IsSelected()) {
				gfx::drawText(vg, x + text_xoffset + 50 / 2, y + (h / 2.f) - (24.f / 2), 24.f, "\uE14B", nullptr, NVG_ALIGN_CENTER | NVG_ALIGN_TOP, theme->GetColour(ThemeEntryID_TEXT_SELECTED));
			}

			m_scroll_name.Draw(vg, selected, x + text_xoffset + 65, y + (h / 2.f), w - (75 + text_xoffset + 65 + 50), 20, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, theme->GetColour(text_id), e.name);

			// NOTE: make this native only if i disable dir scan from above.
			if (e.IsDir()) {
				// NOTE: this takes longer than 16ms when opening a new folder due to it
				// checking all 9 folders at once.
				if (!got_dir_count && e.file_count == -1 && e.dir_count == -1) {
					got_dir_count = true;
					m_fs->DirGetEntryCount(GetNewPath(e), &e.file_count, &e.dir_count);
				}

				if (e.file_count != -1) {
					gfx::drawTextArgs(vg, x + w - text_xoffset, y + (h / 2.f) - 3, 16.f, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM, theme->GetColour(text_id), "%zd files"_i18n.c_str(), e.file_count);
				}
				if (e.dir_count != -1) {
					gfx::drawTextArgs(vg, x + w - text_xoffset, y + (h / 2.f) + 3, 16.f, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP, theme->GetColour(text_id), "%zd dirs"_i18n.c_str(), e.dir_count);
				}
			}
			else if (e.IsFile()) {
				if (!e.time_stamp.is_valid) {
					const auto path = GetNewPath(e);
					if (m_fs->IsNative()) {
						m_fs->GetFileTimeStampRaw(path, &e.time_stamp);
					}
					else {
						m_fs->FileGetSizeAndTimestamp(path, &e.time_stamp, &e.file_size);
					}
				}

				const auto t = (time_t)(e.time_stamp.modified);
				struct tm tm {};
				localtime_r(&t, &tm);
				gfx::drawTextArgs(vg, x + w - text_xoffset, y + (h / 2.f) + 3, 16.f, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP, theme->GetColour(text_id), "%02u/%02u/%u", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
				if ((double)e.file_size / 1024.0 / 1024.0 <= 0.009) {
					gfx::drawTextArgs(vg, x + w - text_xoffset, y + (h / 2.f) - 3, 16.f, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM, theme->GetColour(text_id), "%.2f KiB", (double)e.file_size / 1024.0);
				}
				else {
					gfx::drawTextArgs(vg, x + w - text_xoffset, y + (h / 2.f) - 3, 16.f, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM, theme->GetColour(text_id), "%.2f MiB", (double)e.file_size / 1024.0 / 1024.0);
				}
			}
			});
	}

	void FsView::OnFocusGained() {
		Widget::OnFocusGained();
		if (m_entries.empty()) {
			if (m_path.empty()) {
				Scan(m_fs->Root());
			}
			else {
				Scan(m_path);
			}
		}
	}

	void FsView::SetSide(ViewSide side) {
		m_side = side;

		const auto pos = m_menu->GetPos();
		this->SetPos(pos);
		Vec4 v{ 75, GetY() + 1.f + 42.f, 1220.f - 45.f * 2, 60 };

		if (m_menu->IsSplitScreen()) {
			if (m_side == ViewSide::Left) {
				this->SetW(pos.w / 2 - pos.x / 2);
				this->SetX(pos.x / 2 + 20.f);
			}
			else if (m_side == ViewSide::Right) {
				this->SetW(pos.w / 2 - pos.x / 2);
				this->SetX(pos.x / 2 + SCREEN_WIDTH / 2);
			}

			v.w /= 2;
			v.w -= v.x / 2;

			if (m_side == ViewSide::Left) {
				v.x = v.x / 2 + 20.f;
			}
			else if (m_side == ViewSide::Right) {
				v.x = v.x / 2 + SCREEN_WIDTH / 2;
			}
		}

		m_list = std::make_unique<List>(1, 8, m_pos, v);
		if (m_menu->IsSplitScreen()) {
			m_list->SetPageJump(false);
		}

		// reset scroll position.
		m_scroll_name.Reset();
	}

	void FsView::SetIndex(s64 index) {
		m_index = index;
		if (!m_index) {
			m_list->SetYoff();
		}

		if (IsSd() && !m_entries_current.empty() && !GetEntry().checked_internal_extension && IsSamePath(GetEntry().GetExtension(), "zip")) {
			GetEntry().checked_internal_extension = true;

			TimeStamp ts;
			fs::FsPath filename_inzip{};
			if (R_SUCCEEDED(mz::PeekFirstFileName(GetFs(), GetNewPathCurrent(), filename_inzip))) {
				if (auto ext = std::strrchr(filename_inzip, '.')) {
					GetEntry().internal_name = filename_inzip.toString();
					GetEntry().internal_extension = ext + 1;
				}
				log_write("\tzip, time taken: %.2fs %zums\n", ts.GetSecondsD(), ts.GetMs());
			}
		}

		m_menu->UpdateSubheading();
	}

	void FsView::InstallFiles() {
		if (!App::GetInstallEnable()) {
			App::ShowEnableInstallPrompt();
			return;
		}

		const auto targets = GetSelectedEntries();

		App::Push<OptionBox>("Install Selected files?"_i18n, "No"_i18n, "Yes"_i18n, 0, [this, targets](auto op_index) {
			if (op_index && *op_index) {
				App::PopToMenu();

				App::Push<ui::ProgressBox>(0, "Installing "_i18n, "", [this, targets](auto pbox) -> Result {
					for (auto& e : targets) {
						R_TRY(yati::InstallFromFile(pbox, m_fs.get(), GetNewPath(e)));
						App::Notify("Installed "_i18n + e.GetName());
					}

					R_SUCCEED();
					}, [this](Result rc) {
						App::PushErrorBox(rc, "File install failed!"_i18n);
						});
			}
			});
	}

	void FsView::UnzipFiles(fs::FsPath dir_path) {
		const auto targets = GetSelectedEntries();

		// set to current path.
		if (dir_path.empty()) {
			dir_path = m_path;
		}

		App::Push<ui::ProgressBox>(0, "Extracting "_i18n, "", [this, dir_path, targets](auto pbox) -> Result {
			const auto is_hdd_fs = m_fs->Root().starts_with("ums");

			for (auto& e : targets) {
				pbox->SetTitle(e.GetName());
				const auto zip_out = GetNewPath(e);
				R_TRY(thread::TransferUnzipAll(pbox, zip_out, m_fs.get(), dir_path, nullptr, is_hdd_fs ? thread::Mode::SingleThreaded : thread::Mode::SingleThreadedIfSmaller));
			}

			R_SUCCEED();
			}, [this](Result rc) {
				App::PushErrorBox(rc, "Extract failed!"_i18n);

				if (R_SUCCEEDED(rc)) {
					App::Notify("Extract success!"_i18n);
				}

				Scan(m_path);
				log_write("did extract\n");
				});
	}

	void FsView::ZipFiles(fs::FsPath zip_out) {
		const auto targets = GetSelectedEntries();

		// set to current path.
		if (zip_out.empty()) {
			if (std::size(targets) == 1) {
				const auto name = targets[0].name;
				const auto ext = std::strrchr(targets[0].name, '.');
				fs::FsPath file_path;
				if (!ext) {
					std::snprintf(file_path, sizeof(file_path), "%s.zip", name);
				}
				else {
					std::snprintf(file_path, sizeof(file_path), "%.*s.zip", (int)(ext - name), name);
				}
				zip_out = fs::AppendPath(m_path, file_path);
				log_write("zip out: %s name: %s file_path: %s\n", zip_out.s, name, file_path.s);
			}
			else {
				// loop until we find an unused file name.
				for (u64 i = 0; ; i++) {
					fs::FsPath file_path = "Archive.zip";
					if (i) {
						std::snprintf(file_path, sizeof(file_path), "Archive (%zu).zip", i);
					}

					zip_out = fs::AppendPath(m_path, file_path);
					if (!m_fs->FileExists(zip_out)) {
						break;
					}
				}
			}
		}
		else {
			if (!std::string_view(zip_out).ends_with(".zip")) {
				zip_out += ".zip";
			}
		}

		App::Push<ui::ProgressBox>(0, "Compressing "_i18n, "", [this, zip_out, targets](auto pbox) -> Result {
			const auto t = std::time(NULL);
			const auto tm = std::localtime(&t);
			const auto is_hdd_fs = m_fs->Root().starts_with("ums");

			// pre-calculate the time rather than calculate it in the loop.
			zip_fileinfo zip_info{};
			zip_info.tmz_date.tm_sec = tm->tm_sec;
			zip_info.tmz_date.tm_min = tm->tm_min;
			zip_info.tmz_date.tm_hour = tm->tm_hour;
			zip_info.tmz_date.tm_mday = tm->tm_mday;
			zip_info.tmz_date.tm_mon = tm->tm_mon;
			zip_info.tmz_date.tm_year = tm->tm_year;

			zlib_filefunc64_def file_func;
			mz::FileFuncStdio(&file_func);

			auto zfile = zipOpen2_64(zip_out, APPEND_STATUS_CREATE, nullptr, &file_func);
			R_UNLESS(zfile, Result_ZipOpen2_64);
			ON_SCOPE_EXIT(zipClose(zfile, "npshop v" APP_VERSION_HASH));

			const auto zip_add = [&](const fs::FsPath& file_path) -> Result {
				// the file name needs to be relative to the current directory.
				const char* file_name_in_zip = file_path.s + std::strlen(m_path);

				// strip root path (/ or ums0:)
				if (!std::strncmp(file_name_in_zip, m_fs->Root(), std::strlen(m_fs->Root()))) {
					file_name_in_zip += std::strlen(m_fs->Root());
				}

				// root paths are banned in zips, they will warn when extracting otherwise.
				while (file_name_in_zip[0] == '/') {
					file_name_in_zip++;
				}

				pbox->NewTransfer(file_name_in_zip);

				if (ZIP_OK != zipOpenNewFileInZip(zfile, file_name_in_zip, &zip_info, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION)) {
					log_write("failed to add zip for %s\n", file_path.s);
					R_THROW(Result_ZipOpenNewFileInZip);
				}
				ON_SCOPE_EXIT(zipCloseFileInZip(zfile));

				return thread::TransferZip(pbox, zfile, m_fs.get(), file_path, nullptr, is_hdd_fs ? thread::Mode::SingleThreaded : thread::Mode::SingleThreadedIfSmaller);
				};

			for (auto& e : targets) {
				pbox->SetTitle(e.GetName());
				if (e.IsFile()) {
					const auto file_path = GetNewPath(e);
					R_TRY(zip_add(file_path));
				}
				else {
					FsDirCollections collections;
					get_collections(GetNewPath(e), e.name, collections);

					for (const auto& collection : collections) {
						for (const auto& file : collection.files) {
							const auto file_path = fs::AppendPath(collection.path, file.name);
							R_TRY(zip_add(file_path));
						}
					}
				}
			}

			R_SUCCEED();
			}, [this](Result rc) {
				App::PushErrorBox(rc, "Compress failed!"_i18n);

				if (R_SUCCEEDED(rc)) {
					App::Notify("Compress success!"_i18n);
				}

				Scan(m_path);
				log_write("did compress\n");
				});
	}

	void FsView::UploadFiles() {
		const auto targets = GetSelectedEntries();

		const auto network_locations = location::Load();
		if (network_locations.empty()) {
			App::Notify("No upload locations set!"_i18n);
			return;
		}

		PopupList::Items items;
		for (const auto& p : network_locations) {
			items.emplace_back(p.name);
		}

		App::Push<PopupList>(
			"Select upload location"_i18n, items, [this, network_locations](auto op_index) {
				if (!op_index) {
					return;
				}

				const auto loc = network_locations[*op_index];
				App::Push<ProgressBox>(0, "Uploading"_i18n, "", [this, loc](auto pbox) -> Result {
					auto targets = GetSelectedEntries();
					const auto is_file_based_emummc = App::IsFileBaseEmummc();

					const auto file_add = [&](s64 file_size, const fs::FsPath& file_path, const char* name) -> Result {
						// the file name needs to be relative to the current directory.
						const auto relative_file_name = file_path.s + std::strlen(m_path);
						pbox->SetTitle(name);
						pbox->NewTransfer(relative_file_name);

						fs::File f;
						R_TRY(m_fs->OpenFile(file_path, FsOpenMode_Read, &f));

						return thread::TransferPull(pbox, file_size,
							[&](void* data, s64 off, s64 size, u64* bytes_read) -> Result {
								const auto rc = f.Read(off, data, size, FsReadOption_None, bytes_read);
								if (m_fs->IsNative() && is_file_based_emummc) {
									svcSleepThread(2e+6); // 2ms
								}
								return rc;
							},
							[&](thread::PullCallback pull) -> Result {
								s64 offset{};
								const auto result = curl::Api().FromMemory(
									CURL_LOCATION_TO_API(loc),
									curl::OnProgress{ pbox->OnDownloadProgressCallback() },
									curl::UploadInfo{
										relative_file_name, file_size,
										[&](void* ptr, size_t size) -> size_t {
										// curl will request past the size of the file, causing an error.
										if (offset >= file_size) {
											log_write("finished file upload\n");
											return 0;
										}

										u64 bytes_read{};
										if (R_FAILED(pull(ptr, size, &bytes_read))) {
											log_write("failed to read in custom callback: %zd size: %zd\n", offset, size);
											return 0;
										}

										offset += bytes_read;
										return bytes_read;
									}
									}
								);

								R_UNLESS(result.success, Result_FileBrowserFailedUpload);
								R_SUCCEED();
							}
						);
						};

					for (auto& e : targets) {
						if (e.IsFile()) {
							const auto file_path = GetNewPath(e);
							R_TRY(file_add(e.file_size, file_path, e.GetName().c_str()));
						}
						else {
							FsDirCollections collections;
							get_collections(GetNewPath(e), e.name, collections, true);

							for (const auto& collection : collections) {
								for (const auto& file : collection.files) {
									const auto file_path = fs::AppendPath(collection.path, file.name);
									R_TRY(file_add(file.file_size, file_path, file.name));
								}
							}
						}
					}

					R_SUCCEED();
					}, [this](Result rc) {
						App::PushErrorBox(rc, "Failed to, TODO: add message here"_i18n);
						m_menu->ResetSelection();

						if (R_SUCCEEDED(rc)) {
							App::Notify("Upload successfull!"_i18n);
							log_write("Upload successfull!!!\n");
						}
						else {
							App::Notify("Upload failed!"_i18n);
							log_write("Upload failed!!!\n");
						}
						});
			}
		);
	}

	auto FsView::Scan(const fs::FsPath& new_path, bool is_walk_up) -> Result {
		App::SetBoostMode(true);
		ON_SCOPE_EXIT(App::SetBoostMode(false));

		log_write("new scan path: %s\n", new_path.s);
		if (!is_walk_up && !m_path.empty() && !m_entries_current.empty()) {
			const LastFile f(GetEntry().name, m_index, m_list->GetYoff(), m_entries_current.size());
			m_previous_highlighted_file.emplace_back(f);
		}

		m_path = new_path;
		m_entries.clear();
		m_index = 0;
		m_list->SetYoff(0);
		m_menu->SetTitleSubHeading(m_path);
		m_selected_count = 0;

		fs::Dir d;
		R_TRY(m_fs->OpenDirectory(new_path, FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles, &d));

		// we won't run out of memory here (tm)
		std::vector<FsDirectoryEntry> dir_entries;
		R_TRY(d.ReadAll(dir_entries));

		const auto count = dir_entries.size();
		m_entries.reserve(count);

		m_entries_index.clear();
		m_entries_index_hidden.clear();
		m_entries_index_search.clear();

		m_entries_index.reserve(count);
		m_entries_index_hidden.reserve(count);

		u32 i = 0;
		for (const auto& e : dir_entries) {
			m_entries_index_hidden.emplace_back(i);
			if ('.' != e.name[0]) {
				m_entries_index.emplace_back(i);
			}

			m_entries.emplace_back(e);
			i++;
		}

		Sort();
		SetIndex(0);

		// find previous entry
		if (is_walk_up && !m_previous_highlighted_file.empty()) {
			ON_SCOPE_EXIT(m_previous_highlighted_file.pop_back());
			SetIndexFromLastFile(m_previous_highlighted_file.back());
		}

		R_SUCCEED();
	}

	void FsView::Sort() {
		// returns true if lhs should be before rhs
		const auto sort = m_menu->m_sort.Get();
		const auto order = m_menu->m_order.Get();
		const auto folders_first = m_menu->m_folders_first.Get();
		const auto hidden_last = m_menu->m_hidden_last.Get();

		const auto sorter = [this, sort, order, folders_first, hidden_last](u32 _lhs, u32 _rhs) -> bool {
			const auto& lhs = m_entries[_lhs];
			const auto& rhs = m_entries[_rhs];

			if (hidden_last) {
				if (lhs.IsHidden() && !rhs.IsHidden()) {
					return false;
				}
				else if (!lhs.IsHidden() && rhs.IsHidden()) {
					return true;
				}
			}

			if (folders_first) {
				if (lhs.type == FsDirEntryType_Dir && !(rhs.type == FsDirEntryType_Dir)) { // left is folder
					return true;
				}
				else if (!(lhs.type == FsDirEntryType_Dir) && rhs.type == FsDirEntryType_Dir) { // right is folder
					return false;
				}
			}

			switch (sort) {
			case SortType_Size: {
				if (lhs.file_size == rhs.file_size) {
					return strncasecmp(lhs.name, rhs.name, sizeof(lhs.name)) < 0;
				}
				else if (order == OrderType_Descending) {
					return lhs.file_size > rhs.file_size;
				}
				else {
					return lhs.file_size < rhs.file_size;
				}
			} break;
			case SortType_Alphabetical: {
				if (order == OrderType_Descending) {
					return strncasecmp(lhs.name, rhs.name, sizeof(lhs.name)) < 0;
				}
				else {
					return strncasecmp(lhs.name, rhs.name, sizeof(lhs.name)) > 0;
				}
			} break;
			}

			std::unreachable();
			};

		if (m_menu->m_show_hidden.Get()) {
			m_entries_current = m_entries_index_hidden;
		}
		else {
			m_entries_current = m_entries_index;
		}

		std::sort(m_entries_current.begin(), m_entries_current.end(), sorter);
	}

	void FsView::SortAndFindLastFile(bool scan) {
		std::optional<LastFile> last_file;
		if (!m_path.empty() && !m_entries_current.empty()) {
			last_file = LastFile(GetEntry().name, m_index, m_list->GetYoff(), m_entries_current.size());
		}

		if (scan) {
			Scan(m_path);
		}
		else {
			Sort();
		}

		if (last_file.has_value()) {
			SetIndexFromLastFile(*last_file);
		}
	}

	void FsView::SetIndexFromLastFile(const LastFile& last_file) {
		SetIndex(0);

		s64 index = -1;
		for (u64 i = 0; i < m_entries_current.size(); i++) {
			if (last_file.name == GetEntry(i).name) {
				index = i;
				break;
			}
		}
		if (index >= 0) {
			if (index == last_file.index && m_entries_current.size() == last_file.entries_count) {
				m_list->SetYoff(last_file.offset);
				log_write("index is the same as last time\n");
			}
			else {
				// file position changed!
				log_write("file position changed\n");
				// guesstimate where the position is
				if (index >= 8) {
					m_list->SetYoff(((index - 8) + 1) * m_list->GetMaxY());
				}
				else {
					m_list->SetYoff(0);
				}
			}
			SetIndex(index);
		}
	}

	void FsView::OnDeleteCallback() {
		bool use_progress_box{ true };

		// check if we only have 1 file / folder
		if (m_menu->m_selected.m_files.size() == 1) {
			const auto& entry = m_menu->m_selected.m_files[0];
			const auto full_path = GetNewPath(m_menu->m_selected.m_path, entry.name);

			if (entry.IsDir()) {
				bool empty{};
				m_fs->IsDirEmpty(full_path, &empty);
				if (empty) {
					if (auto rc = m_fs->DeleteDirectory(full_path); R_FAILED(rc)) {
						App::PushErrorBox(rc, "Failed to delete directory"_i18n);
					}
					use_progress_box = false;
				}
			}
			else {
				if (auto rc = m_fs->DeleteFile(full_path); R_FAILED(rc)) {
					App::PushErrorBox(rc, "Failed to delete file"_i18n);
				}
				use_progress_box = false;
			}
		}

		if (!use_progress_box) {
			m_menu->RefreshViews();
			log_write("did delete\n");
		}
		else {
			App::Push<ProgressBox>(0, "Deleting"_i18n, "", [this](auto pbox) -> Result {
				FsDirCollections collections;
				auto& selected = m_menu->m_selected;
				auto src_fs = selected.m_view->GetFs();

				// build list of dirs / files
				for (const auto& p : selected.m_files) {
					pbox->Yield();
					R_TRY(pbox->ShouldExitResult());

					const auto full_path = GetNewPath(selected.m_path, p.name);
					if (p.IsDir()) {
						pbox->NewTransfer("Scanning "_i18n + full_path);
						R_TRY(get_collections(src_fs, full_path, p.name, collections));
					}
				}

				return DeleteAllCollectionsWithSelected(pbox, src_fs, selected, collections);
				}, [this](Result rc) {
					App::PushErrorBox(rc, "Failed to, TODO: add message here"_i18n);

					m_menu->RefreshViews();
					log_write("did delete\n");
					});
		}
	}

	void FsView::OnPasteCallback() {
		// check if we only have 1 file / folder and is cut (rename)
		if (m_menu->m_selected.SameFs(this) && m_menu->m_selected.m_files.size() == 1 && m_menu->m_selected.m_type == SelectedType::Cut) {
			const auto& entry = m_menu->m_selected.m_files[0];
			const auto full_path = GetNewPath(m_menu->m_selected.m_path, entry.name);

			if (entry.IsDir()) {
				m_fs->RenameDirectory(full_path, GetNewPath(entry));
			}
			else {
				m_fs->RenameFile(full_path, GetNewPath(entry));
			}

			m_menu->RefreshViews();
		}
		else {
			App::Push<ProgressBox>(0, "Pasting"_i18n, "", [this](auto pbox) -> Result {
				auto& selected = m_menu->m_selected;
				auto src_fs = selected.m_view->GetFs();
				const auto is_same_fs = selected.SameFs(this);

				if (selected.SameFs(this) && selected.m_type == SelectedType::Cut) {
					for (const auto& p : selected.m_files) {
						pbox->Yield();
						R_TRY(pbox->ShouldExitResult());

						const auto src_path = GetNewPath(selected.m_path, p.name);
						const auto dst_path = GetNewPath(m_path, p.name);

						pbox->SetTitle(p.name);
						pbox->NewTransfer("Pasting "_i18n + src_path);

						if (p.IsDir()) {
							m_fs->RenameDirectory(src_path, dst_path);
						}
						else {
							m_fs->RenameFile(src_path, dst_path);
						}
					}
				}
				else {
					FsDirCollections collections;

					const auto on_paste_file = [&](auto& src_path, auto& dst_path) -> Result {
						if (selected.m_type == SelectedType::Cut) {
							// update timestamp if possible.
							if (!m_fs->IsNative()) {
								FsTimeStampRaw ts;
								if (R_SUCCEEDED(src_fs->GetFileTimeStampRaw(src_path, &ts))) {
									m_fs->SetTimestamp(dst_path, &ts);
								}
							}

							// delete src file. folders are removed after.
							R_TRY(src_fs->DeleteFile(src_path));
						}

						R_SUCCEED();
						};

					// build list of dirs / files
					for (const auto& p : selected.m_files) {
						pbox->Yield();
						R_TRY(pbox->ShouldExitResult());

						const auto full_path = GetNewPath(selected.m_path, p.name);
						if (p.IsDir()) {
							pbox->NewTransfer("Scanning "_i18n + full_path);
							R_TRY(get_collections(src_fs, full_path, p.name, collections));
						}
					}

					for (const auto& p : selected.m_files) {
						pbox->Yield();
						R_TRY(pbox->ShouldExitResult());

						const auto src_path = GetNewPath(selected.m_path, p.name);
						const auto dst_path = GetNewPath(p);

						if (p.IsDir()) {
							pbox->SetTitle(p.name);
							pbox->NewTransfer("Creating "_i18n + dst_path);
							m_fs->CreateDirectory(dst_path);
						}
						else {
							pbox->SetTitle(p.name);
							pbox->NewTransfer("Copying "_i18n + src_path);
							R_TRY(pbox->CopyFile(src_fs, m_fs.get(), src_path, dst_path, is_same_fs));
							R_TRY(on_paste_file(src_path, dst_path));
						}
					}

					// copy everything in collections
					for (const auto& c : collections) {
						const auto base_dst_path = GetNewPath(m_path, c.parent_name);

						for (const auto& p : c.dirs) {
							pbox->Yield();
							R_TRY(pbox->ShouldExitResult());

							// const auto src_path = GetNewPath(c.path, p.name);
							const auto dst_path = GetNewPath(base_dst_path, p.name);

							pbox->SetTitle(p.name);
							pbox->NewTransfer("Creating "_i18n + dst_path);
							m_fs->CreateDirectory(dst_path);
						}

						for (const auto& p : c.files) {
							pbox->Yield();
							R_TRY(pbox->ShouldExitResult());

							const auto src_path = GetNewPath(c.path, p.name);
							const auto dst_path = GetNewPath(base_dst_path, p.name);

							pbox->SetTitle(p.name);
							pbox->NewTransfer("Copying "_i18n + src_path);
							R_TRY(pbox->CopyFile(src_fs, m_fs.get(), src_path, dst_path, is_same_fs));
							R_TRY(on_paste_file(src_path, dst_path));
						}
					}

					// moving accross fs is not possible, thus files have to be copied.
					// this leaves the files on the src_fs.
					// the files are deleted one by one after a successfull copy (see above)
					// however this leaves the folders.
					// the folders cannot be deleted until the end as they have to be removed in
					// reverse order so that the folder can be deleted (it must be empty).
					if (selected.m_type == SelectedType::Cut) {
						R_TRY(DeleteAllCollectionsWithSelected(pbox, src_fs, selected, collections, FsDirOpenMode_ReadDirs));
					}
				}

				R_SUCCEED();
				}, [this](Result rc) {
					App::PushErrorBox(rc, "Failed to, TODO: add message here"_i18n);

					m_menu->RefreshViews();
					log_write("did paste\n");
					});
		}
	}

	void FsView::OnRenameCallback() {

	}

	auto FsView::get_collection(fs::Fs* fs, const fs::FsPath& path, const fs::FsPath& parent_name, FsDirCollection& out, bool inc_file, bool inc_dir, bool inc_size) -> Result {
		out.path = path;
		out.parent_name = parent_name;

		const auto fetch = [fs, &path](std::vector<FsDirectoryEntry>& out, u32 flags) -> Result {
			fs::Dir d;
			R_TRY(fs->OpenDirectory(path, flags, &d));
			return d.ReadAll(out);
			};

		if (inc_file) {
			u32 flags = FsDirOpenMode_ReadFiles;
			if (!inc_size) {
				flags |= FsDirOpenMode_NoFileSize;
			}
			R_TRY(fetch(out.files, flags));
		}

		if (inc_dir) {
			R_TRY(fetch(out.dirs, FsDirOpenMode_ReadDirs));
		}

		R_SUCCEED();
	}

	auto FsView::get_collections(fs::Fs* fs, const fs::FsPath& path, const fs::FsPath& parent_name, FsDirCollections& out, bool inc_size) -> Result {
		// get a list of all the files / dirs
		FsDirCollection collection;
		R_TRY(get_collection(fs, path, parent_name, collection, true, true, inc_size));
		log_write("got collection: %s parent_name: %s files: %zu dirs: %zu\n", path.s, parent_name.s, collection.files.size(), collection.dirs.size());
		out.emplace_back(collection);

		// for (size_t i = 0; i < collection.dirs.size(); i++) {
		for (const auto& p : collection.dirs) {
			// use heap as to not explode the stack
			const auto new_path = std::make_unique<fs::FsPath>(FsView::GetNewPath(path, p.name));
			const auto new_parent_name = std::make_unique<fs::FsPath>(FsView::GetNewPath(parent_name, p.name));
			log_write("trying to get nested collection: %s parent_name: %s\n", new_path->s, new_parent_name->s);
			R_TRY(get_collections(fs, *new_path, *new_parent_name, out, inc_size));
		}

		R_SUCCEED();
	}

	auto FsView::get_collection(const fs::FsPath& path, const fs::FsPath& parent_name, FsDirCollection& out, bool inc_file, bool inc_dir, bool inc_size) -> Result {
		return get_collection(m_fs.get(), path, parent_name, out, true, true, inc_size);
	}

	auto FsView::get_collections(const fs::FsPath& path, const fs::FsPath& parent_name, FsDirCollections& out, bool inc_size) -> Result {
		return get_collections(m_fs.get(), path, parent_name, out, inc_size);
	}

	Result FsView::DeleteAllCollections(ProgressBox* pbox, fs::Fs* fs, const FsDirCollections& collections, u32 mode) {
		// delete everything in collections, reversed
		for (const auto& c : std::views::reverse(collections)) {
			const auto delete_func = [&](auto& array) -> Result {
				for (const auto& p : array) {
					pbox->Yield();
					R_TRY(pbox->ShouldExitResult());

					const auto full_path = FsView::GetNewPath(c.path, p.name);
					pbox->SetTitle(p.name);
					pbox->NewTransfer("Deleting "_i18n + full_path.toString());
					if ((mode & FsDirOpenMode_ReadDirs) && p.type == FsDirEntryType_Dir) {
						log_write("deleting dir: %s\n", full_path.s);
						R_TRY(fs->DeleteDirectory(full_path));
						svcSleepThread(1e+5);
					}
					else if ((mode & FsDirOpenMode_ReadFiles) && p.type == FsDirEntryType_File) {
						log_write("deleting file: %s\n", full_path.s);
						R_TRY(fs->DeleteFile(full_path));
						svcSleepThread(1e+5);
					}
				}

				R_SUCCEED();
				};

			R_TRY(delete_func(c.files));
			R_TRY(delete_func(c.dirs));
		}

		R_SUCCEED();
	}

	static Result DeleteAllCollectionsWithSelected(ProgressBox* pbox, fs::Fs* fs, const SelectedStash& selected, const FsDirCollections& collections, u32 mode = FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles) {
		R_TRY(FsView::DeleteAllCollections(pbox, fs, collections, mode));

		for (const auto& p : selected.m_files) {
			pbox->Yield();
			R_TRY(pbox->ShouldExitResult());

			const auto full_path = FsView::GetNewPath(selected.m_path, p.name);
			pbox->SetTitle(p.name);
			pbox->NewTransfer("Deleting "_i18n + full_path.toString());

			if ((mode & FsDirOpenMode_ReadDirs) && p.type == FsDirEntryType_Dir) {
				log_write("deleting dir: %s\n", full_path.s);
				R_TRY(fs->DeleteDirectory(full_path));
			}
			else if ((mode & FsDirOpenMode_ReadFiles) && p.type == FsDirEntryType_File) {
				log_write("deleting file: %s\n", full_path.s);
				R_TRY(fs->DeleteFile(full_path));
			}
		}

		R_SUCCEED();
	}

	void FsView::SetFs(const fs::FsPath& new_path, const FsEntry& new_entry) {
		if (m_fs && m_fs_entry.root == new_entry.root && m_fs_entry.type == new_entry.type) {
			log_write("same fs, ignoring\n");
			return;
		}

		// m_fs.reset();
		m_path = new_path;
		m_entries.clear();
		m_entries_index.clear();
		m_entries_index_hidden.clear();
		m_entries_index_search.clear();
		m_entries_current = {};
		m_previous_highlighted_file.clear();
		m_menu->m_selected.Reset();
		m_selected_count = 0;
		m_fs_entry = new_entry;

		switch (new_entry.type) {
		case FsType::Sd:
			m_fs = std::make_unique<fs::FsNativeSd>(m_menu->m_ignore_read_only.Get());
			break;
		case FsType::ImageNand:
			m_fs = std::make_unique<fs::FsNativeImage>(FsImageDirectoryId_Nand);
			break;
		case FsType::ImageSd:
			m_fs = std::make_unique<fs::FsNativeImage>(FsImageDirectoryId_Sd);
			break;
		case FsType::Stdio:
			m_fs = std::make_unique<fs::FsStdio>(true, new_entry.root);
			break;
		}

		if (HasFocus()) {
			if (m_path.empty()) {
				Scan(m_fs->Root());
			}
			else {
				Scan(m_path);
			}
		}
	}

	void FsView::DisplayHash(hash::Type type) {
		// hack because we cannot share output between threaded calls...
		static std::string hash_out;
		hash_out.clear();

		App::Push<ProgressBox>(0, "Hashing"_i18n, GetEntry().name, [this, type](auto pbox) -> Result {
			const auto full_path = GetNewPathCurrent();
			pbox->NewTransfer(full_path);
			R_TRY(hash::Hash(pbox, type, m_fs.get(), full_path, hash_out));

			R_SUCCEED();
			}, [this, type](Result rc) {
				App::PushErrorBox(rc, "Failed to hash file..."_i18n);

				if (R_SUCCEEDED(rc)) {
					char buf[0x100];
					// std::snprintf(buf, sizeof(buf), "%s\n%s\n%s", hash::GetTypeStr(type), hash_out.c_str(), GetEntry().GetName());
					std::snprintf(buf, sizeof(buf), "%s\n%s", hash::GetTypeStr(type), hash_out.c_str());
					App::Push<OptionBox>(buf, "OK"_i18n);
				}
				});
	}

	void FsView::DisplayOptions() {
		auto options = std::make_unique<Sidebar>("File Options"_i18n, Sidebar::Side::RIGHT);
		ON_SCOPE_EXIT(App::Push(std::move(options)));

		options->Add<SidebarEntryCallback>("Sort By"_i18n, [this]() {
			auto options = std::make_unique<Sidebar>("Sort Options"_i18n, Sidebar::Side::RIGHT);
			ON_SCOPE_EXIT(App::Push(std::move(options)));

			SidebarEntryArray::Items sort_items;
			sort_items.push_back("Size"_i18n);
			sort_items.push_back("Alphabetical"_i18n);

			SidebarEntryArray::Items order_items;
			order_items.push_back("Descending"_i18n);
			order_items.push_back("Ascending"_i18n);

			options->Add<SidebarEntryArray>("Sort"_i18n, sort_items, [this](s64& index_out) {
				m_menu->m_sort.Set(index_out);
				SortAndFindLastFile();
				}, m_menu->m_sort.Get());

			options->Add<SidebarEntryArray>("Order"_i18n, order_items, [this](s64& index_out) {
				m_menu->m_order.Set(index_out);
				SortAndFindLastFile();
				}, m_menu->m_order.Get());

			options->Add<SidebarEntryBool>("Show Hidden"_i18n, m_menu->m_show_hidden.Get(), [this](bool& v_out) {
				m_menu->m_show_hidden.Set(v_out);
				SortAndFindLastFile();
				});

			options->Add<SidebarEntryBool>("Folders First"_i18n, m_menu->m_folders_first.Get(), [this](bool& v_out) {
				m_menu->m_folders_first.Set(v_out);
				SortAndFindLastFile();
				});

			options->Add<SidebarEntryBool>("Hidden Last"_i18n, m_menu->m_hidden_last.Get(), [this](bool& v_out) {
				m_menu->m_hidden_last.Set(v_out);
				SortAndFindLastFile();
				});
			});

		if (m_entries_current.size()) {
			options->Add<SidebarEntryCallback>("Cut"_i18n, [this]() {
				m_menu->AddSelectedEntries(SelectedType::Cut);
				}, true);

			options->Add<SidebarEntryCallback>("Copy"_i18n, [this]() {
				m_menu->AddSelectedEntries(SelectedType::Copy);
				}, true);
		}

		if (!m_menu->m_selected.Empty() && (m_menu->m_selected.Type() == SelectedType::Cut || m_menu->m_selected.Type() == SelectedType::Copy)) {
			options->Add<SidebarEntryCallback>("Paste"_i18n, [this]() {
				const std::string buf = "Paste file(s)?"_i18n;
				App::Push<OptionBox>(
					buf, "No"_i18n, "Yes"_i18n, 0, [this](auto op_index) {
						if (op_index && *op_index) {
							App::PopToMenu();
							OnPasteCallback();
						}
					});
				});
		}

		// can't rename more than 1 file
		if (m_entries_current.size() && !m_selected_count) {
			options->Add<SidebarEntryCallback>("Rename"_i18n, [this]() {
				std::string out;
				const auto& entry = GetEntry();
				const auto name = entry.GetName();
				if (R_SUCCEEDED(swkbd::ShowText(out, "Set New File Name"_i18n.c_str(), name.c_str())) && !out.empty() && out != name) {
					App::PopToMenu();

					const auto src_path = GetNewPath(entry);
					const auto dst_path = GetNewPath(m_path, out);

					Result rc;
					if (entry.IsFile()) {
						rc = m_fs->RenameFile(src_path, dst_path);
					}
					else {
						rc = m_fs->RenameDirectory(src_path, dst_path);
					}

					if (R_SUCCEEDED(rc)) {
						Scan(m_path);
					}
					else {
						const auto msg = std::string("Failed to rename file: ") + entry.name;
						App::PushErrorBox(rc, msg);
					}
				}
				});
		}

		if (m_entries_current.size()) {
			options->Add<SidebarEntryCallback>("Delete"_i18n, [this]() {
				m_menu->AddSelectedEntries(SelectedType::Delete);

				log_write("clicked on delete\n");
				App::Push<OptionBox>(
					"Delete Selected files?"_i18n, "No"_i18n, "Yes"_i18n, 0, [this](auto op_index) {
						if (op_index && *op_index) {
							App::PopToMenu();
							OnDeleteCallback();
						}
					}
				);
				log_write("pushed delete\n");
				});
		}

		// returns true if all entries match the ext array.
		const auto check_all_ext = [this](auto& exts) {
			const auto entries = GetSelectedEntries();
			for (auto& e : entries) {
				if (!IsExtension(e.GetExtension(), exts)) {
					return false;
				}
			}
			return true;
			};

		// if install is enabled, check if all currently selected files are installable.
		if (m_entries_current.size()) {
			if (check_all_ext(INSTALL_EXTENSIONS)) {
				auto entry = options->Add<SidebarEntryCallback>("Install"_i18n, [this]() {
					InstallFiles();
					});
				entry->Depends(App::GetInstallEnable, i18n::get(App::INSTALL_DEPENDS_STR), App::ShowEnableInstallPrompt);
			}
		}

		if (m_entries_current.size()) {
			if (check_all_ext(ZIP_EXTENSIONS)) {
				options->Add<SidebarEntryCallback>("Extract zip"_i18n, [this]() {
					auto options = std::make_unique<Sidebar>("Extract Options"_i18n, Sidebar::Side::RIGHT);
					ON_SCOPE_EXIT(App::Push(std::move(options)));

					options->Add<SidebarEntryCallback>("Extract here"_i18n, [this]() {
						UnzipFiles("");
						});

					options->Add<SidebarEntryCallback>("Extract to root"_i18n, [this]() {
						App::Push<OptionBox>("Are you sure you want to extract to root?"_i18n,
							"No"_i18n, "Yes"_i18n, 0, [this](auto op_index) {
								if (op_index && *op_index) {
									UnzipFiles(m_fs->Root());
								}
							});
						});

					options->Add<SidebarEntryCallback>("Extract to..."_i18n, [this]() {
						std::string out;
						if (R_SUCCEEDED(swkbd::ShowText(out, "Enter the path to the folder to extract into", fs::AppendPath(m_path, ""))) && !out.empty()) {
							UnzipFiles(out);
						}
						});
					});
			}

			if (!check_all_ext(ZIP_EXTENSIONS) || m_selected_count) {
				options->Add<SidebarEntryCallback>("Compress to zip"_i18n, [this]() {
					auto options = std::make_unique<Sidebar>("Compress Options"_i18n, Sidebar::Side::RIGHT);
					ON_SCOPE_EXIT(App::Push(std::move(options)));

					options->Add<SidebarEntryCallback>("Compress"_i18n, [this]() {
						ZipFiles("");
						});

					options->Add<SidebarEntryCallback>("Compress to..."_i18n, [this]() {
						std::string out;
						if (R_SUCCEEDED(swkbd::ShowText(out, "Enter the path to the folder to extract into", m_path)) && !out.empty()) {
							ZipFiles(out);
						}
						});
					});
			}
		}

		options->Add<SidebarEntryCallback>("Advanced"_i18n, [this]() {
			DisplayAdvancedOptions();
			});
	}

	void FsView::DisplayAdvancedOptions() {
		auto options = std::make_unique<Sidebar>("Advanced Options"_i18n, Sidebar::Side::RIGHT);
		ON_SCOPE_EXIT(App::Push(std::move(options)));

		SidebarEntryArray::Items mount_items;
		std::vector<FsEntry> fs_entries;

		const auto stdio_locations = location::GetStdio(false);
		for (const auto& e : stdio_locations) {
			u32 flags{};
			if (e.write_protect) {
				flags |= FsEntryFlag_ReadOnly;
			}

			fs_entries.emplace_back(e.name, e.mount, FsType::Stdio, flags);
			mount_items.push_back(e.name);
		}

		for (const auto& e : FS_ENTRIES) {
			fs_entries.emplace_back(e);
			mount_items.push_back(i18n::get(e.name));
		}

		options->Add<SidebarEntryArray>("Mount"_i18n, mount_items, [this, fs_entries](s64& index_out) {
			App::PopToMenu();
			SetFs(fs_entries[index_out].root, fs_entries[index_out]);
			}, i18n::get(m_fs_entry.name));

		options->Add<SidebarEntryCallback>("Create File"_i18n, [this]() {
			std::string out;
			if (R_SUCCEEDED(swkbd::ShowText(out, "Set File Name"_i18n.c_str(), fs::AppendPath(m_path, ""))) && !out.empty()) {
				App::PopToMenu();

				fs::FsPath full_path;
				if (out.starts_with(m_fs_entry.root.s)) {
					full_path = out;
				}
				else {
					full_path = fs::AppendPath(m_path, out);
				}

				m_fs->CreateDirectoryRecursivelyWithPath(full_path);
				if (R_SUCCEEDED(m_fs->CreateFile(full_path, 0, 0))) {
					log_write("created file: %s\n", full_path.s);
					Scan(m_path);
				}
				else {
					log_write("failed to create file: %s\n", full_path.s);
				}
			}
			});

		options->Add<SidebarEntryCallback>("Create Folder"_i18n, [this]() {
			std::string out;
			if (R_SUCCEEDED(swkbd::ShowText(out, "Set Folder Name"_i18n.c_str(), fs::AppendPath(m_path, ""))) && !out.empty()) {
				App::PopToMenu();

				fs::FsPath full_path;
				if (out.starts_with(m_fs_entry.root.s)) {
					full_path = out;
				}
				else {
					full_path = fs::AppendPath(m_path, out);
				}

				if (R_SUCCEEDED(m_fs->CreateDirectoryRecursively(full_path))) {
					log_write("created dir: %s\n", full_path.s);
					Scan(m_path);
				}
				else {
					log_write("failed to create dir: %s\n", full_path.s);
				}
			}
			});

		if (IsSd() && m_entries_current.size() && !m_selected_count && GetEntry().IsFile() && GetEntry().file_size < 1024 * 64) {
			options->Add<SidebarEntryCallback>("View as text (unfinished)"_i18n, [this]() {
				App::Push<fileview::Menu>(GetNewPathCurrent());
				});
		}

		if (m_entries_current.size()) {
			options->Add<SidebarEntryCallback>("Upload"_i18n, [this]() {
				UploadFiles();
				});
		}

		if (m_entries_current.size() && !m_selected_count && GetEntry().IsFile()) {
			options->Add<SidebarEntryCallback>("Hash"_i18n, [this]() {
				auto options = std::make_unique<Sidebar>("Hash Options"_i18n, Sidebar::Side::RIGHT);
				ON_SCOPE_EXIT(App::Push(std::move(options)));

				options->Add<SidebarEntryCallback>("CRC32"_i18n, [this]() {
					DisplayHash(hash::Type::Crc32);
					});
				options->Add<SidebarEntryCallback>("MD5"_i18n, [this]() {
					DisplayHash(hash::Type::Md5);
					});
				options->Add<SidebarEntryCallback>("SHA1"_i18n, [this]() {
					DisplayHash(hash::Type::Sha1);
					});
				options->Add<SidebarEntryCallback>("SHA256"_i18n, [this]() {
					DisplayHash(hash::Type::Sha256);
					});
				});
		}

		options->Add<SidebarEntryBool>("Ignore read only"_i18n, m_menu->m_ignore_read_only.Get(), [this](bool& v_out) {
			m_menu->m_ignore_read_only.Set(v_out);
			m_fs->SetIgnoreReadOnly(v_out);
			});
	}

	Menu::Menu(u32 flags) : MenuBase{ "FileBrowser"_i18n, flags } {
		SetAction(Button::L3, Action{ "Split"_i18n, [this]() {
			SetSplitScreen(IsSplitScreen() ^ 1);
		} });

		if (!IsTab()) {
			SetAction(Button::SELECT, Action{ "Close"_i18n, [this]() {
				PromptIfShouldExit();
			} });
		}

		view_left = std::make_unique<FsView>(this, ViewSide::Left);
		view = view_left.get();
		ueventCreate(&g_change_uevent, true);
	}

	Menu::~Menu() {
	}

	void Menu::Update(Controller* controller, TouchInfo* touch) {
		if (R_SUCCEEDED(waitSingle(waiterForUEvent(&g_change_uevent), 0))) {
			if (IsSplitScreen()) {
				view_left->SortAndFindLastFile(true);
				view_right->SortAndFindLastFile(true);
			}
			else {
				view->SortAndFindLastFile(true);
			}
		}

		// workaround the buttons not being display properly.
		// basically, inherit all actions from the view, draw them,
		// then restore state after.
		const auto view_actions = view->GetActions();
		m_actions.insert_range(view_actions);
		ON_SCOPE_EXIT(RemoveActions(view_actions));

		MenuBase::Update(controller, touch);
		view->Update(controller, touch);
	}

	void Menu::Draw(NVGcontext* vg, Theme* theme) {
		// see Menu::Update().
		const auto view_actions = view->GetActions();
		m_actions.insert_range(view_actions);
		ON_SCOPE_EXIT(RemoveActions(view_actions));

		MenuBase::Draw(vg, theme);

		if (IsSplitScreen()) {
			view_left->Draw(vg, theme);
			view_right->Draw(vg, theme);

			if (view == view_left.get()) {
				gfx::drawRect(vg, view_right->GetPos(), theme->GetColour(ThemeEntryID_FOCUS), 5);
			}
			else {
				gfx::drawRect(vg, view_left->GetPos(), theme->GetColour(ThemeEntryID_FOCUS), 5);
			}

			gfx::drawRect(vg, SCREEN_WIDTH / 2, GetY(), 1, GetH(), theme->GetColour(ThemeEntryID_LINE));
		}
		else {
			view->Draw(vg, theme);
		}
	}

	void Menu::OnFocusGained() {
		MenuBase::OnFocusGained();

		if (IsSplitScreen()) {
			view_left->OnFocusGained();
			view_right->OnFocusGained();
		}
		else {
			view->OnFocusGained();
		}
	}

	void Menu::UpdateSubheading() {
		const auto index = view->m_entries_current.empty() ? 0 : view->m_index + 1;
		this->SetSubHeading(std::to_string(index) + " / " + std::to_string(view->m_entries_current.size()));
	}

	void Menu::SetSplitScreen(bool enable) {
		if (m_split_screen != enable) {
			m_split_screen = enable;

			if (m_split_screen) {
				const auto change_view = [this](FsView* new_view) {
					if (view != new_view) {
						view->OnFocusLost();
						view = new_view;
						view->OnFocusGained();
						SetTitleSubHeading(view->m_path);
						UpdateSubheading();
					}
					};

				// load second screen as a copy of the left side.
				view->SetSide(ViewSide::Left);
				view_right = std::make_unique<FsView>(this, view->m_path, view->GetFsEntry(), ViewSide::Right);
				change_view(view_right.get());

				SetAction(Button::LEFT, Action{ [this, change_view]() {
					change_view(view_left.get());
				} });
				SetAction(Button::RIGHT, Action{ [this, change_view]() {
					change_view(view_right.get());
				} });
			}
			else {
				if (view == view_right.get()) {
					view_left = std::move(view_right);
				}

				view_right = {};
				view = view_left.get();
				view->SetSide(ViewSide::Left);

				RemoveAction(Button::LEFT);
				RemoveAction(Button::RIGHT);
				ResetSelection();
			}
		}
	}

	void Menu::RefreshViews() {
		ResetSelection();

		if (IsSplitScreen()) {
			view_left->Scan(view_left->m_path);
			view_right->Scan(view_right->m_path);
		}
		else {
			view->Scan(view->m_path);
		}
	}

	void Menu::PromptIfShouldExit() {
		if (IsTab()) {
			return;
		}

		App::Push<ui::OptionBox>(
			"Close FileBrowser?"_i18n,
			"No"_i18n, "Yes"_i18n, 1, [this](auto op_index) {
				if (op_index && *op_index) {
					SetPop();
				}
			}
		);
	}

} // namespace npshop::ui::menu::filebrowser