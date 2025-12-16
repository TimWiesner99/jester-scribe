#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

using namespace std;

// Arduino String compatibility layer for testing
class String {
public:
    string data;

    String() {}
    String(const char* str) : data(str) {}
    String(const string& str) : data(str) {}
    String(int num) : data(to_string(num)) {}

    int length() const { return data.length(); }

    char charAt(int index) const {
        if (index >= 0 && index < data.length())
            return data[index];
        return 0;
    }

    String substring(int start) const {
        if (start >= data.length()) return String("");
        return String(data.substr(start));
    }

    String substring(int start, int end) const {
        if (start >= data.length()) return String("");
        if (end > data.length()) end = data.length();
        if (end <= start) return String("");
        return String(data.substr(start, end - start));
    }

    int indexOf(const char* str) const {
        size_t pos = data.find(str);
        return (pos == string::npos) ? -1 : pos;
    }

    int indexOf(const char* str, int from) const {
        size_t pos = data.find(str, from);
        return (pos == string::npos) ? -1 : pos;
    }

    void replace(const char* find, const char* replace) {
        string findStr(find);
        string replaceStr(replace);
        size_t pos = 0;
        while ((pos = data.find(findStr, pos)) != string::npos) {
            data.replace(pos, findStr.length(), replaceStr);
            pos += replaceStr.length();
        }
    }

    void trim() {
        // Trim leading whitespace
        size_t start = data.find_first_not_of(" \t\n\r");
        if (start == string::npos) {
            data = "";
            return;
        }

        // Trim trailing whitespace
        size_t end = data.find_last_not_of(" \t\n\r");
        data = data.substr(start, end - start + 1);
    }

    const char* c_str() const { return data.c_str(); }

    friend ostream& operator<<(ostream& os, const String& s) {
        os << s.data;
        return os;
    }
};

// Copy the helper functions from main_program.cpp
String decodeHTMLEntities(String text) {
  // Common German HTML entities
  text.replace("&quot;", "\"");
  text.replace("&amp;", "&");
  text.replace("&lt;", "<");
  text.replace("&gt;", ">");
  text.replace("&ouml;", "ö");
  text.replace("&auml;", "ä");
  text.replace("&uuml;", "ü");
  text.replace("&Ouml;", "Ö");
  text.replace("&Auml;", "Ä");
  text.replace("&Uuml;", "Ü");
  text.replace("&szlig;", "ß");
  text.replace("&nbsp;", " ");

  return text;
}

String stripHTMLTags(String text) {
  String result = "";
  bool inTag = false;

  for (int i = 0; i < text.length(); i++) {
    char c = text.charAt(i);

    if (c == '<') {
      inTag = true;
      // Check if it's a <br> tag and convert to space
      if (text.substring(i, i + 4).data == "<br>" || text.substring(i, i + 5).data == "<br/>") {
        result.data += " ";
      }
    } else if (c == '>') {
      inTag = false;
    } else if (!inTag) {
      result.data += c;
    }
  }

  return result;
}

int main() {
    // Read the test file
    ifstream file("C:\\Users\\tim20\\Software Projects\\jester-scribe\\tests\\html_trimming.txt");
    if (!file.is_open()) {
        cout << "Error: Could not open test file" << endl;
        return 1;
    }

    stringstream buffer;
    buffer << file.rdbuf();
    string content = buffer.str();
    file.close();

    // Find the HTML input section
    size_t htmlStart = content.find("html input:");
    size_t htmlEnd = content.find("desired output:");

    if (htmlStart == string::npos || htmlEnd == string::npos) {
        cout << "Error: Could not find test sections" << endl;
        return 1;
    }

    string htmlInput = content.substr(htmlStart + 11, htmlEnd - htmlStart - 11);

    // Trim leading/trailing whitespace from input
    size_t start = htmlInput.find_first_not_of(" \t\n\r");
    size_t end = htmlInput.find_last_not_of(" \t\n\r");
    if (start != string::npos && end != string::npos) {
        htmlInput = htmlInput.substr(start, end - start + 1);
    }

    cout << "=== HTML INPUT ===" << endl;
    cout << htmlInput << endl << endl;

    // Simulate the extraction process
    String htmlContent(htmlInput);
    String joke = "";

    // Extract content from <div id="witzdestages">
    int divStart = htmlContent.indexOf("<div id=\"witzdestages\">");
    if (divStart == -1) {
        divStart = htmlContent.indexOf("<div id='witzdestages'>");
    }

    cout << "divStart: " << divStart << endl;

    if (divStart != -1) {
        // Find the closing </div>
        int divEnd = htmlContent.indexOf("</div>", divStart);
        cout << "divEnd: " << divEnd << endl;

        if (divEnd != -1) {
            // Extract content between tags
            joke = htmlContent.substring(divStart, divEnd);
            cout << "After substring extraction: " << joke.length() << " chars" << endl;
            cout << "Content: " << joke << endl << endl;

            // Remove the opening div tag
            int contentStart = joke.indexOf(">") + 1;
            joke = joke.substring(contentStart);
            cout << "After removing opening tag: " << joke.length() << " chars" << endl;
            cout << "Content: " << joke << endl << endl;

            // Remove the footer link span (if present)
            int linkStart = joke.indexOf("<span id=\"witzdestageslink\">");
            if (linkStart == -1) {
                linkStart = joke.indexOf("<span id='witzdestageslink'>");
            }
            if (linkStart != -1) {
                joke = joke.substring(0, linkStart);
            }
            cout << "After removing footer: " << joke.length() << " chars" << endl;
            cout << "Content: " << joke << endl << endl;

            // Decode HTML entities (ö, ä, ü, ß, etc.)
            joke = decodeHTMLEntities(joke);
            cout << "After decoding entities: " << joke.length() << " chars" << endl;
            cout << "Content: " << joke << endl << endl;

            // Remove remaining HTML tags (<br />, etc.)
            joke = stripHTMLTags(joke);
            cout << "After stripping tags: " << joke.length() << " chars" << endl;
            cout << "Content: " << joke << endl << endl;

            // Clean up whitespace
            joke.trim();
            cout << "After trim: " << joke.length() << " chars" << endl;

            // Replace multiple spaces with single space
            while (joke.indexOf("  ") != -1) {
                joke.replace("  ", " ");
            }
            cout << "After space cleanup: " << joke.length() << " chars" << endl;

            cout << endl << "=== FINAL RESULT ===" << endl;
            cout << joke << endl << endl;
            cout << "Length: " << joke.length() << " chars" << endl;
        }
    }

    return 0;
}
