#ifndef GUI_DIALOGS_PINYIN2_HPP_INCLUDED
#define GUI_DIALOGS_PINYIN2_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

namespace gui2 {

class tlabel;
class tlistbox;

class tpinyin2: public tdialog
{
public:
	explicit tpinyin2();

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void reload_errors(tlistbox& list, int pinyins, const std::vector<std::string>& errors);
	void click_generate_pinyinrsp();
	void click_play();
	void click_frequence_text2code();
	void click_4sort_text2code();

	std::string version_str() const;

private:
	const std::string pinyinrsp_path_;
	const std::string rspfile_;
	const std::string test_file_;
	const std::string frequence_file_;
	const std::string pinyin_4sort_file_;

	tlabel* ver_widget_;
	tlistbox* errors_list_;
};

} // namespace gui2

#endif

