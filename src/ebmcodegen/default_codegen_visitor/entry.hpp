/*license*/
auto entry_point = module_.get_entry_point();
if (!entry_point) {
    return unexpect_error("No entry point found");
}
MAYBE(result, visit_Statement(*this, *entry_point));
root.write_unformatted(result);
futils::wrap::cerr_wrap() << program_name << ": Code Generated\n";